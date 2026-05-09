# Pengu Loader — macOS Port

This document specifies the macOS injection model for v1.2.0 and supersedes the macOS portions of [`design.md`](./design.md) §2.1 and §2.3. The implementation lives in the new .NET 10 host described in [`app-hub.md`](./app-hub.md); the deprecated Tauri/Rust loader at [`packages/hub/src-tauri/`](../packages/hub/src-tauri/) does not get this redesign.

This is the **empirically validated** design as of 2026-05-08. Earlier drafts of this document proposed a mach-port runtime-attach plan (`task_for_pid` + `thread_create_running`) and a `launchctl setenv` plan; both were tested in [`app/Pengu.MacOS.Test/`](../app/Pengu.MacOS.Test/) and proved unworkable on Apple Silicon (Rosetta thread-state + macOS DYLD filter respectively). Those approaches are recorded in [§7 Failed approaches](#7-failed-approaches) so the reasoning is preserved.

---

## 1. Background

The v1.2.0 macOS *OnDemand* model patches `League of Legends.app/Contents/Frameworks/Chromium Embedded Framework.framework/Libraries/libEGL.dylib` in place via `insert_dylib`, adding an `LC_LOAD_DYLIB` for `core.dylib`, and restores the backup on LCUX exit. CEF then loads `core.dylib` into both the LCUX browser process and its Helper renderers naturally.

That model worked while RCS scanned the LoL bundle once, at install/update time. RCS now scans bundle files **on every launch**. Any modification to `libEGL.dylib` triggers a "Repair files" prompt and rolls back the patch before LCUX starts.

The new macOS *Universal* mode touches no file inside `/Applications/League of Legends.app/`.

---

## 2. The lever

Codesign state on the current LCUX build (TeamID `K832E2UXV7`):

| Binary | Flags | Notes |
| --- | --- | --- |
| `RiotClientServices` | `0x10000(runtime)` — hardened | Strips `DYLD_*` from envp at exec |
| `Riot Client` (inner UI) | `0x10000(runtime)` — hardened | |
| `LeagueClient` (LoL launcher) | `0x0(none)` | |
| `LeagueClientUx` (browser) | `0x0(none)` | |
| `LeagueClientUx Helper*` | `0x0(none)` | All variants: GPU, Renderer, utility |

**No hardened runtime on any LCUX-side process.** This means, on this specific target:

- `dyld` does **not** strip `DYLD_*` env vars at exec.
- Library validation is **not** enforced — `dlopen` of an unsigned or ad-hoc-signed `core.dylib` succeeds.
- Children spawned by LCUX (every CEF Helper) inherit `DYLD_INSERT_LIBRARIES` automatically via dyld.

The whole approach is conditional on `flags=0x0` on the LCUX-side processes. See [§6.1](#61-riot-enables-hardened-runtime-on-lcux).

---

## 3. Activation model — kill-and-respawn with `DYLD_INSERT_LIBRARIES`

This becomes the macOS *Universal* mode and is the new default. The legacy `insert_dylib` flow remains as an *OnDemand* fallback for users who hit attach failures.

```
RCS spawns LeagueClient (the launcher, flags=0x0)
LeagueClient binds Foundation/LCDS server on 127.0.0.1:<app-port>
LeagueClient spawns LCUX (pid X)
   ┌──────  Pengu.MacOS daemon polls proc_listpids every 5 ms
   │        catches pid X within 5 ms (pre-cef_initialize)
   │        kill(X, SIGSTOP)
   │        sysctl(KERN_PROCARGS2) → argv + envp
   │        parse --install-directory= from argv → cwd
   │        posix_spawn LCUX with same argv + envp
   │            + DYLD_INSERT_LIBRARIES=/abs/path/core.dylib
   │            + WorkingDirectory = install-directory
   │        DO NOT kill X — leave it SIGSTOP'd forever (see §3.5)
   ▼
LCUX (new pid Y) starts under Pengu.MacOS as parent
   dyld loads core.dylib (flags=0x0 → DYLD_INSERT_LIBRARIES honored)
   core.dylib constructor runs HookBrowserProcess()
LCUX connects to LeagueClient's Foundation server (handshake succeeds)
LCUX spawns Helpers; each inherits DYLD_INSERT_LIBRARIES via dyld
   GPU / utility Helpers: core.dylib loads, constructor short-circuits
   Renderer Helpers: core.dylib runs HookRendererProcess()
Riot Client UI is fully operational
```

The shape mirrors the Windows IFEO model functionally: detection trigger, replace the spawn, dyld inheritance handles the helper fan-out. The primitives are different (signals + sysctl + posix_spawn instead of registry-driven IFEO), but the contract is identical.

### 3.1 Detection — `proc_listpids` polling

`kqueue NOTE_TRACK` on `RiotClientServices` returns `ENOTSUP` because RCS is hardened. Same-UID polling of `proc_listpids` at a 5 ms interval catches every new LCUX pid within 5 ms of `posix_spawn`, well before `cef_initialize` (which is roughly 50–200 ms into LCUX startup). No entitlement, no kqueue-tracking privilege.

### 3.2 SIGSTOP, then re-verify path

Once a new pid passes the basename + path-suffix filter (matching `LeagueClientUx`), the daemon immediately calls `kill(pid, SIGSTOP)`. **After the SIGSTOP, the daemon re-reads `proc_pidpath` and confirms the path is still LCUX.** This catches the fork-then-exec race: a child process briefly inherits its parent's executable path between `fork()` and `execve()`, so polling can land on a transient mid-fork moment when a Helper-to-be still appears as `LeagueClientUx`. The post-SIGSTOP re-check sees the post-`exec` path; if it's now `Helper (Renderer)`, `/usr/bin/profiles`, etc., the daemon `SIGCONT`s and skips the catch.

### 3.3 Read argv + envp via `sysctl(KERN_PROCARGS2)`

`sysctl` with `mib = {CTL_KERN, KERN_PROCARGS2, pid}` returns a binary buffer containing the target's `argc`, executable path, argv, and envp. Same-UID processes can read this without entitlements. The C# parser is in [`app/Pengu.MacOS.Test/ProcArgs.cs`](../app/Pengu.MacOS.Test/ProcArgs.cs) and ports straight into `Pengu.MacOS`.

### 3.4 Working directory — `--install-directory`

LCUX uses **relative** paths internally (e.g. `Plugins/`). RCS sets the cwd to the value of `--install-directory` (which is also passed in argv) before exec. If we re-spawn with the daemon's cwd instead, LCUX's plugin discovery walks the wrong tree and segfaults during `rcp-fe-plugin-runner` initialization.

Parse `--install-directory=` from the captured argv and pass it as `ProcessStartInfo.WorkingDirectory`. Empirically required.

### 3.5 Don't `SIGKILL` the original — leave it `SIGSTOP`'d

This is the non-obvious step that makes the whole approach work. Naïvely, the flow would be SIGSTOP → read → SIGKILL → respawn. But:

- `LeagueClient` watches its child LCUX via `SIGCHLD` + `waitpid`.
- If the original LCUX dies, LeagueClient sees `SIGCHLD`, waits, gets the exit status, and **tears down its Foundation/LCDS HTTP server** because "child crashed."
- Our re-spawned LCUX comes up, tries to fetch `https://riot:<token>@127.0.0.1:<app-port>/bootstrap.html` from LeagueClient's Foundation server, gets `ERR_CONNECTION_REFUSED` (server gone), error page → broken UI.

By leaving the original LCUX `SIGSTOP`'d (never `SIGCONT`'d, never `SIGKILL`'d):

- LeagueClient probes its child via `kill(pid, 0)` → returns success on stopped processes → LeagueClient still considers child "alive."
- Foundation server keeps running.
- Our re-spawned LCUX (different pid, same auth token from argv) connects to Foundation, handshakes, operates normally.

The original LCUX is frozen in early dyld init (we caught it within 5 ms of exec), hasn't bound any ports, hasn't loaded most of its image. It consumes a few KB of stopped-process state until the user logs out. Cosmetic-only cost; if it ever matters, an end-of-session cleanup pass can `SIGKILL` orphaned stopped LCUX pids whose parents are gone.

### 3.6 Helpers — pure dyld inheritance, no in-process hook

Once `core.dylib` is loaded into LCUX (the browser process) via `DYLD_INSERT_LIBRARIES`, every CEF Helper that LCUX `posix_spawn`s inherits the env var natively through dyld. Each Helper is also `flags=0x0`, so dyld honors the variable and loads `core.dylib` into the Helper at startup. `core.dylib`'s constructor uses `getprogname()` + argv inspection (mirroring the Windows dispatcher in [dllmain.cc](../core/src/dllmain.cc)) and runs `HookRendererProcess()` in `Helper (Renderer)`, no-ops in `Helper (GPU)` and `Helper` (utility).

**No `posix_spawn` interpose is required inside `core.dylib`** — earlier drafts proposed this, but it's redundant on macOS because dyld already does the work. The Windows-equivalent `CreateProcessW` hook is only needed because Windows doesn't propagate IFEO to children.

---

## 4. Implementation flow in `app/Pengu.MacOS/`

The macOS head doesn't exist yet (only `app/Pengu/`, `app/Pengu.Gen/`, `app/Pengu.Windows/`). Bring it up first per [`app-hub.md`](./app-hub.md) §2:

```
app/Pengu.MacOS/
  Pengu.MacOS.csproj           net10.0-macos, AOT-published
  Program.cs                   AppDelegate + NSApplication.Init
  Window/BorderlessWindow.cs   NSWindow shell — see app-hub.md §6
  Browser/WkWebViewHost.cs     WKWebView + WKURLSchemeHandler for app://
  Bridge/JsBridge.cs           WKScriptMessageHandler glue
  Activation/
    InsertDylibAction.cs       OnDemand fallback (legacy libEGL patch)
    RespawnAction.cs           Universal mode (this design)
  Native/
    Mach.cs                    Native imports for kill / proc_listpids / proc_pidpath / sysctl
    ProcArgs.cs                sysctl(KERN_PROCARGS2) reader
    LcuxWatcher.cs             5 ms proc_listpids polling loop
```

The C# code from the POC at [`app/Pengu.MacOS.Test/`](../app/Pengu.MacOS.Test/) ports directly: `ProcArgs.cs` is unchanged, the polling loop becomes `LcuxWatcher.cs`, the catch routine becomes `RespawnAction.OnSessionCreatedAsync`.

### Step 1 — `Pengu.MacOS.Native.Mach`

`LibraryImport` declarations against `/usr/lib/libSystem.dylib`:
- `kill(pid_t, int)` for `SIGSTOP` / `SIGCONT`
- `proc_listpids(uint type, uint typeinfo, IntPtr buffer, int buffersize)`
- `proc_pidpath(int pid, IntPtr buffer, uint buffersize)`
- `sysctl(int*, uint, void*, ulong*, void*, ulong)` for `KERN_PROCARGS2`

All AOT-clean. No `DllImport`. ~50 LOC.

### Step 2 — `Pengu.MacOS.Native.ProcArgs.Read(int pid)`

Wraps `sysctl(KERN_PROCARGS2)`. Returns `(string ExePath, string[] Argv, string[] Envp)`. Buffer-format parsing is straightforward: `int32 argc` → null-terminated exe path → padding nulls → argc null-terminated argv strings → null-terminated envp strings until empty entry. Reference: [`app/Pengu.MacOS.Test/ProcArgs.cs`](../app/Pengu.MacOS.Test/ProcArgs.cs). ~60 LOC.

### Step 3 — `Pengu.MacOS.Activation.LcuxWatcher`

Maintains a `HashSet<int>` of seen pids (snapshotted at start). Polls `proc_listpids(PROC_ALL_PIDS)` every 5 ms, diffs against the seen set, and for each new pid:

1. `proc_pidpath` → check basename `LeagueClientUx` and full path ends with `/League of Legends.app/Contents/MacOS/LeagueClientUx`.
2. Pass match to `RespawnAction.OnSessionCreatedAsync`.

Add the daemon's own respawn pid to the seen set so we don't catch our own spawn. ~80 LOC.

### Step 4 — `Pengu.MacOS.Activation.RespawnAction.OnSessionCreatedAsync`

```csharp
public Task OnSessionCreatedAsync(int pid, string path, CancellationToken ct)
{
    // 1. Pre-cef_initialize: SIGSTOP within 5 ms of exec.
    if (Mach.kill(pid, Signals.SIGSTOP) != 0) return Task.CompletedTask;

    // 2. Fork-window race guard: re-check path after the stop.
    if (ProcPath(pid) != path)
    {
        Mach.kill(pid, Signals.SIGCONT);
        return Task.CompletedTask;
    }

    // 3. Read argv + envp from the stopped process.
    var snap = ProcArgs.Read(pid);

    // 4. Find --install-directory= for cwd (LCUX uses relative plugin paths).
    string? installDir = snap.Argv
        .FirstOrDefault(a => a.StartsWith("--install-directory=", StringComparison.Ordinal))
       ?["--install-directory=".Length..];

    // 5. Re-spawn with the same argv + envp + DYLD_INSERT_LIBRARIES.
    var psi = new ProcessStartInfo
    {
        FileName         = snap.ExePath,
        WorkingDirectory = installDir ?? Path.GetDirectoryName(snap.ExePath) ?? "",
        UseShellExecute  = false,
        CreateNoWindow   = false,
    };
    foreach (var a in snap.Argv.Skip(1)) psi.ArgumentList.Add(a);
    psi.Environment.Clear();
    foreach (var e in snap.Envp)
    {
        int eq = e.IndexOf('=');
        if (eq > 0) psi.Environment[e[..eq]] = e[(eq + 1)..];
    }
    psi.Environment["DYLD_INSERT_LIBRARIES"] = _coreDylibPath;
    var proc = Process.Start(psi)!;

    // 6. Do NOT kill the original. LeagueClient's SIGCHLD-based child watch
    //    would tear down Foundation; leave original SIGSTOP'd forever (§3.5).
    return Task.CompletedTask;
}
```

### Step 5 — Wire into the activation registry

Register `RespawnAction` for `(Platform.MacOS, ActivationMode.Universal)` in `ActivationActionRegistry`. The legacy `InsertDylibAction` (per [`app-hub.md`](./app-hub.md) §9.5) stays registered for `(Platform.MacOS, ActivationMode.OnDemand)` as the fallback for users who hit `RespawnAction` failures (e.g. if Riot ever changes the chain).

The shared C# RCS WAMP daemon ([`app-hub.md`](./app-hub.md) §9.7) is **not needed** for Universal mode — the `LcuxWatcher` polling loop is its replacement. The WAMP daemon is still useful for OnDemand (it tells the daemon when to apply/restore the libEGL patch).

---

## 5. What `core.dylib` does NOT need on macOS

The earlier draft of this document proposed a `posix_spawn` / `posix_spawnp` dyld interposer inside `core.dylib` to inject Helpers. **That is no longer required.** Helpers inherit `DYLD_INSERT_LIBRARIES` natively because:

- Every Helper bundle is `flags=0x0` (no hardened runtime).
- LCUX is `flags=0x0` (does not strip `DYLD_*` from its environ).
- `posix_spawn` defaults to inheriting the parent's environ.

`core.dylib` only needs the existing host-name dispatch in [dllmain.cc](../core/src/dllmain.cc): if `getprogname() == "LeagueClientUx"` → `HookBrowserProcess()`; if matching `LeagueClientUx Helper (Renderer)` and argv contains `--type=renderer` → `HookRendererProcess()`; else early return.

This is a meaningful simplification compared to the Windows side, where `core.dll` *does* need a `CreateProcessW` hook to chase renderer children.

---

## 6. Configuration, signing, footprint

- **No new config keys.** Activation mode toggle is the existing `Universal` / `OnDemand` enum from `app-hub.md`.
- **Default on macOS:** `Universal` (this design). `OnDemand` retained as user-selectable fallback.
- **`core.dylib` signing:** ad-hoc (`codesign -s - core.dylib`) is sufficient. Library validation is off on LCUX, so the dylib doesn't need a Developer ID or matching Team ID. Ad-hoc avoids per-release notarization on the dylib.
- **Pengu.app signing:** Developer-ID + notarization as usual (Tauri did this; .NET 10 host follows the same pattern).
- **No sudo, no SIP changes, no entitlements** required for the user.
- **No Riot file modified** at any point. Universal mode does not touch `/Applications/League of Legends.app/` or `/Users/Shared/Riot Games/`.
- **Cosmetic side effect:** one `(stopped)` LeagueClientUx per LoL launch persists in `ps -ax` until the user logs out. Optional periodic cleanup: `SIGKILL` stopped LCUX pids whose stop time is more than ~5 minutes old and whose parent is no longer running.

---

## 7. Failure modes

### 7.1 Riot enables hardened runtime on LCUX

If `LeagueClientUx` ever ships with `flags=0x10000(runtime)`, dyld will strip `DYLD_INSERT_LIBRARIES` from envp at exec; the entire approach dies. Helper bundles becoming hardened breaks the inheritance fan-out separately. Watch-and-wait risk; not designed around today.

Fallback path: revert to OnDemand (`InsertDylibAction`) and accept the repair prompt as a permanent macOS limitation.

### 7.2 LeagueClient changes its child-tracking mechanism

The "leave SIGSTOP'd" trick relies on LeagueClient using `SIGCHLD`/`waitpid` (which a stopped process doesn't trigger) and `kill(pid, 0)` liveness checks. If LeagueClient ever switches to heartbeat-based monitoring (e.g. expecting LCDS pings from its child), a stopped original would time out and LeagueClient would tear down anyway.

Detect via the `ERR_CONNECTION_REFUSED` symptom returning. Mitigation: also `SIGSTOP` LeagueClient during the swap so it can't observe the original's stopped state until our new LCUX is up; or pivot to targeting LeagueClient (kill+respawn the launcher instead).

### 7.3 Race lost — LCUX reaches `cef_initialize` before our SIGSTOP

5 ms polling is empirically tight enough on the tested machine, but slow disks or contended scheduling could miss. Symptom: `core.dylib`'s `cef_initialize` hook never fires (since cef_initialize already ran in the original LCUX before SIGSTOP, but we're respawning anyway, so this isn't actually an issue for the kill-and-respawn model — the *new* LCUX's `cef_initialize` is what we hook). True risk is much smaller than for runtime-attach designs.

### 7.4 `ProcessStartInfo.Environment` filters or modifies envp

.NET's `Process.Start` is implemented over `posix_spawn` on macOS and passes `envp` straight through. Empirically verified that `DYLD_INSERT_LIBRARIES` reaches LCUX. If a future .NET runtime adds defensive filtering of `DYLD_*`, switch to a direct `LibraryImport` of `posix_spawn` with manual envp marshaling. Reference recipe in [`app/Pengu.MacOS.Test/Native.cs`](../app/Pengu.MacOS.Test/Native.cs).

### 7.5 Cosmetic: zombie SIGSTOP'd LCUX pids accumulate

Each LoL launch leaves one stopped LCUX pid behind. Over many launches in a single login session, `ps -ax` gets cluttered. Practical impact is near-zero (each pid is ~1 MB resident in early-dyld state). End-of-session cleanup (a `pkill -SIGKILL -SIGSTOP -f LeagueClientUx` on Pengu daemon shutdown) handles this if it ever matters.

---

## 8. Failed approaches (recorded so we don't re-derive)

For completeness, these were tested and rejected during development. Memory file `~/.claude/projects/.../memory/project_pengu_macos_injection.md` has the empirical detail.

1. **Mach-port runtime attach** (`task_for_pid` + `mach_vm_allocate` + `thread_create_running` with `x86_THREAD_STATE64` + remote `dlopen` shellcode). Blocked: even with sudo, `thread_set_state` on a kernel-created Rosetta thread fails `KERN_INVALID_ARGUMENT`. Rosetta's per-thread translation hooks must be installed by its in-process libpthread interposer at thread-creation time; no public API path from outside.

2. **`launchctl setenv DYLD_INSERT_LIBRARIES`**. macOS silently filters `DYLD_*` (and other security-sensitive prefixes) at `launchctl setenv` since Big Sur. Reproducible: `PENGU_TEST_NORMAL` round-trips via `setenv`/`getenv`; `DYLD_INSERT_LIBRARIES` does not.

3. **`posix_spawn` Riot Client / RCS with inline `DYLD_INSERT_LIBRARIES=...`** or **LaunchAgent plist `EnvironmentVariables`**. The env reaches the binary's exec but RCS's hardened-runtime dyld strips it before main; LeagueClient and LCUX never see it.

4. **`Info.plist.LSEnvironment`**. macOS specifically blocks `DYLD_*` keys here.

5. **Shell out to `lldb`** (`lldb -p <pid> -o 'expr (void*)dlopen(...)' -o detach -o q`). *Works* — lldb is Apple-signed with `cs.debugger`, sidesteps every kernel gate. Rejected because it requires Xcode CLI Tools as a dependency, takes 1–3 s per attach (too slow for sub-`cef_initialize` injection windows), and requires a one-time `DevToolsSecurity` user-prompt the first time the daemon attaches. Kill-and-respawn beats it on every dimension once we figured out the "leave original SIGSTOP'd" trick.

6. **Kill-and-respawn LCUX with `SIGKILL`**. Original cleaner-looking design; broke LeagueClient's `SIGCHLD`-based child tracking. Replaced with the leave-SIGSTOP'd-forever pattern in §3.5.

7. **Target LeagueClient instead of LCUX for kill+respawn**. Briefly explored. Worked in principle but moved the same coordination break one link upstream (RCS↔LeagueClient). LCUX is the right target as long as we don't kill it.

---

## 9. References

- [`docs/design.md`](./design.md) — v1.2.0 overall architecture.
- [`docs/app-hub.md`](./app-hub.md) — the .NET 10 host this lives in.
- [`app/Pengu.MacOS.Test/`](../app/Pengu.MacOS.Test/) — empirical POC of every claim in this doc.
- v1.1.6 production loader (Windows-only WPF/.NET Framework, IFEO injection) — historical reference for the per-platform activation pattern.
