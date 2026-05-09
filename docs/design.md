# Pengu Loader — Design

This document describes the architecture of Pengu Loader as it stands on the `main` branch (v1.2.0, unreleased). It supersedes the earlier `HOW_IT_WORKS.md` (last updated for v0.6.0) and is written for contributors to Pengu itself; it also covers enough of the runtime surface to be useful to plugin authors.

> **Version map.** Production today still ships **v1.1.6**, a Windows-only WPF/.NET Framework loader that uses IFEO injection and disables plugins by renaming `index.js` to `index.js_`. **v1.2.0** is the unreleased cross-platform rewrite documented here. The Tauri-based loader UI in [`packages/hub/`](../packages/hub/) is itself transitional — the planned final form is a from-scratch .NET 10 host (WebView2 on Windows, WKWebView on macOS) living in [`app/`](../app/), at which point `packages/hub/` will be reduced to the SolidJS frontend it serves.

---

## 1. Overview

Pengu Loader is a plugin loader for the **League of Legends Client UX** (LCUX), a CEF (Chromium Embedded Framework) based desktop app. It loads JavaScript plugins from disk into the running LCUX renderer, exposes a small native API surface to those plugins, and adds quality-of-life hooks (DevTools, transparency, hotkeys, super-potato mode, etc.).

The project is split into three components:

| Component | Path | Language | Role |
| --- | --- | --- | --- |
| **Core** | [`core/`](../core/) | C++20 | Native module loaded into LCUX's CEF processes (browser + renderer). Owns every CEF, libcef, and OS-level hook. |
| **Preload** | [`packages/preload/`](../packages/preload/) | TypeScript (Vite/IIFE) | A single bundled script embedded into the core. Runs inside the renderer's V8 context to bootstrap the plugin runtime, expose JS APIs, and load user plugins. |
| **Loader** | [`packages/hub/`](../packages/hub/) (Tauri, transitional) → [`app/`](../app/) (.NET 10, planned) | Rust + SolidJS today | A standalone GUI app that installs/uninstalls the core, manages plugins, and watches for LCUX launches on macOS. **Not loaded into LCUX itself.** |

The core is the only component that actually runs inside LCUX. The preload is *embedded* into the core at build time and executed as JavaScript in the main renderer's V8 context. The loader is just an installer/manager.

```
┌─────────────────────────────────────────────────────────────┐
│ Riot Client Services (RCS)                                  │
│   spawns LCUX with --app-port=<rand> --app-token=<rand>     │
└────────────────┬────────────────────────────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────────────────────────────┐
│ LeagueClientUx.exe          (CEF browser process)           │
│   ╔═══════════════════════════════════════════════════════╗ │
│   ║ core.{dll,dylib} loaded via IFEO / symlink / dylib    ║ │
│   ║   • hooks libcef exports (cef_initialize, ...)        ║ │
│   ║   • registers https://plugins/ and https://riotclient ║ │
│   ║   • injects itself into renderer children             ║ │
│   ║   • window manipulation + transparency                ║ │
│   ║   • DevTools, hotkeys, IPC                            ║ │
│   ╚═══════════════════════════════════════════════════════╝ │
└────────────────┬────────────────────────────────────────────┘
                 │  spawns
                 ▼
┌─────────────────────────────────────────────────────────────┐
│ LeagueClientUxRender.exe --type=renderer                    │
│   ╔═══════════════════════════════════════════════════════╗ │
│   ║ same core module (injected via CreateProcessW hook)   ║ │
│   ║   • hooks cef_execute_process → OnContextCreated      ║ │
│   ║   • exposes window.Pengu, window.__native, window.os  ║ │
│   ║   • runs embedded preload.js                          ║ │
│   ║     → loads each plugin via                           ║ │
│   ║       import('https://plugins/<entry>')               ║ │
│   ╚═══════════════════════════════════════════════════════╝ │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. Core module

### 2.1 Process targeting

The core is a single shared library (`core.dll` on Windows, `core.dylib` on macOS) that handles both CEF roles by inspecting the host executable name on load.

In [`dllmain.cc`](../core/src/dllmain.cc):

- If the host exe name contains `LeagueClientUx.exe` (Windows) or is `LeagueClientUx` (macOS), `HookBrowserProcess()` runs.
- If the host is `LeagueClientUxRender.exe` (Windows) or `LeagueClientUx Helper (Renderer)` (macOS) **and** the command line includes `--type=renderer`, `HookRendererProcess()` runs.
- Anything else (sandbox helpers, GPU helpers, crashpad) is left alone.

On Windows the browser process additionally hooks `CreateProcessW` to detect renderer children being spawned. When a `LeagueClientUxRender.exe ... --type=renderer` is created, the hook:

1. Adds `CREATE_SUSPENDED` to the creation flags.
2. After the call returns, uses `VirtualAllocEx` + `WriteProcessMemory` + `CreateRemoteThread` with `LoadLibraryW` to inject the core DLL into the child.
3. Resumes the thread.

This is needed because IFEO only fires on the parent process — children don't inherit it. The renderer DLL has to be injected explicitly.

On macOS the daemon catches `LeagueClientUx` at spawn and re-spawns it with `DYLD_INSERT_LIBRARIES=core.dylib` (see [§2.3](#23-activation-paths) below). LCUX is `flags=0x0(none)` so dyld honors the env var; the renderer Helpers are also `flags=0x0` and inherit the env var natively when LCUX `posix_spawn`s them, so no `CreateProcessW`-equivalent is needed on macOS — dyld inheritance does the work that the Windows hook does explicitly.

### 2.2 CEF binding style — no libcef_wrapper

Pengu uses CEF's **C API** (`cef_*_t` structs of function pointers) directly rather than the `libcef_wrapper` C++ wrapper. The reasons are documented in [`pengu.h:32-75`](../core/src/pengu.h#L32-L75):

- Performance: avoids the wrapper's virtual-call layer.
- Hooking: function pointers in C structs are easier to swap than vtable methods.
- Distribution: no need to ship `libcef_wrapper.lib` and match its compiler/runtime.

The `cef_bind_method` macro and `CefRefCount<T>` template in [`pengu.h`](../core/src/pengu.h) let C++ classes plug member functions straight into C-API function-pointer slots, with a `static_assert` to catch ABI drift when CEF headers change.

### 2.3 Activation paths

How does `core.dll/dylib` get loaded into LCUX in the first place? Three modes, exposed in the loader UI as `Universal`, `Targeted`, `OnDemand`:

#### Windows — IFEO (Universal, default)

[`packages/hub/src-tauri/src/windows/mod_ifeo.rs`](../packages/hub/src-tauri/src/windows/mod_ifeo.rs) writes a registry value:

```
HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\
    LeagueClientUx.exe\
        Debugger = rundll32 "C:\path\to\core.dll", #6000
```

When Windows launches `LeagueClientUx.exe`, the IFEO `Debugger` value redirects through `rundll32`, which loads `core.dll` and calls its ordinal-6000 export. That export ([`_BootstrapEntry` in `dllmain.cc`](../core/src/dllmain.cc#L120-L159)):

1. Re-launches the original `LeagueClientUx.exe` with `CREATE_SUSPENDED | DEBUG_ONLY_THIS_PROCESS`.
2. Strips the debugger flag via `NtRemoveProcessDebug` (so the process doesn't actually run under a debugger — `DEBUG_ONLY_THIS_PROCESS` is just used to prevent IFEO from re-firing).
3. Injects `core.dll` into the new process via `CreateRemoteThread → LoadLibraryW`.
4. Resumes the suspended thread.

This is loosely the same idea as the original Mecha injector (per the v0.6.0 doc) but uses `rundll32` rather than Detours and immediately removes the debugger to avoid CEF performance penalties.

Vanguard does **not** scan or block this path: LCUX and `LeagueClientUxRender.exe` are explicitly scope-excluded from Vanguard's anti-cheat boundary, and `core.dll` itself lives outside the LoL install directory.

#### Windows — symlink + dllproxy (Targeted, fallback)

Some environments don't allow IFEO (locked-down domain policies, AV interference, etc.). The fallback in [`mod_symlink.rs`](../packages/hub/src-tauri/src/windows/mod_symlink.rs) creates a symlink in the LoL install folder pointing at `core.dll`:

```
<LoL>\version.dll  ───►  C:\path\to\core.dll
```

`version.dll` is the chosen target because LCUX statically links against it (so it loads on startup), and it's not a name Vanguard cares about. The legacy alternatives `d3d9.dll` and `dwrite.dll` still work but are less preferred. (The v0.6.0 doc described d3d9 proxying; that approach is functionally subsumed by the more flexible `version.dll` shape.)

Because `LoadLibrary("version.dll")` will resolve to our DLL instead of `C:\Windows\System32\version.dll`, [`dllproxy.cc`](../core/src/dllproxy.cc) re-exports every symbol the real `version.dll` provides (`GetFileVersionInfo*`, `VerQueryValue*`, etc.) and forwards them to the system DLL on first call. `d3d9.dll` and `dwrite.dll` exports are also forwarded for compatibility.

Symlink mode requires admin or Windows Developer Mode for the initial install, but uninstall does not.

#### macOS — kill-and-respawn with `DYLD_INSERT_LIBRARIES` (Universal, default)

For full design and rejected alternatives see [`macos-port.md`](./macos-port.md). Empirically validated on 2026-05-08 against macOS Sequoia + Apple Silicon + Rosetta-translated LCUX.

The lever: `LeagueClientUx`, every Helper bundle, and `LeagueClient` (the launcher between RCS and LCUX) all sign with `flags=0x0(none)` — no hardened runtime — so dyld honors `DYLD_INSERT_LIBRARIES` and library validation is off. `RiotClientServices` *is* hardened (so dyld strips DYLD env vars from its environ before it spawns children, which kills any "set the env upstream and let it propagate" scheme), but we don't need to inject into RCS.

When the user enables Pengu, the loader (running in the tray) starts an `LcuxWatcher` that polls `proc_listpids` every 5 ms. On a new pid matching `LeagueClientUx`:

1. `kill(pid, SIGSTOP)` immediately — pre-`cef_initialize`.
2. Re-check `proc_pidpath` to skip fork-window false positives (a transient child briefly inheriting LCUX's exec path before its own `execve`).
3. `sysctl(KERN_PROCARGS2)` reads the original's argv + envp (same-UID, no entitlement).
4. Parse `--install-directory=` from argv → working directory.
5. `posix_spawn` a replacement `LeagueClientUx` with the same argv + envp + an added `DYLD_INSERT_LIBRARIES=/abs/path/core.dylib` and the captured cwd.
6. **Do not `SIGKILL` the original.** A stopped pid passes `kill(pid, 0)` liveness, so `LeagueClient`'s `SIGCHLD`-based child watch never fires and its Foundation/LCDS server stays up; killing it would trigger Foundation teardown and our re-spawn would get `ERR_CONNECTION_REFUSED` on `bootstrap.html`. Original LCUX stays frozen in early dyld init (we caught it within 5 ms), no ports bound, ~1 MB resident, persists until logout.

dyld in the new LCUX honors `DYLD_INSERT_LIBRARIES`, loads `core.dylib`, runs its constructor → `HookBrowserProcess()`. When LCUX `posix_spawn`s its Helpers, they inherit the env var, dyld loads `core.dylib` into them too, the host-name dispatch in [`dllmain.cc`](../core/src/dllmain.cc) routes to `HookRendererProcess()` for renderers / no-ops for GPU and utility helpers.

No sudo, no SIP relaxation, no `cs.debugger` entitlement, no Riot file modified.

Notable rejected alternatives, kept here so they don't get re-derived: mach-port runtime attach (`task_for_pid` works under sudo but `thread_set_state x86_THREAD_STATE64` on a fresh kernel-created Rosetta thread fails `KERN_INVALID_ARGUMENT`); `launchctl setenv DYLD_INSERT_LIBRARIES` (macOS silently filters `DYLD_*` from launchctl); spawn-Riot-Client-with-DYLD-inline or LaunchAgent `EnvironmentVariables` (RCS hardened-runtime strips at the chain entry); shelling out to lldb (works, but Xcode CLI Tools dependency + 1–3 s attach + DevToolsSecurity prompt). See [`macos-port.md`](./macos-port.md) §8 for the full audit.

#### macOS — insert_dylib (OnDemand, fallback)

Retained as a fallback for users who hit Universal-mode failure. Same flow as the legacy v1.2.0 model: subscribe to RCS WAMP, on `league_of_legends` session create, back up `libEGL.dylib` and patch it in place with an `LC_LOAD_DYLIB` for `core.dylib`; restore on session delete. Triggers Riot's per-launch repair prompt — known UX cost; the only viable path if a future LCUX build enables hardened runtime and breaks Universal.

OnDemand is a defined enum value on Windows too but is not wired up — IFEO is strictly more reliable there.

### 2.4 CEF version pinning

[`libcef.cc`](../core/src/libcef.cc) refuses to attach if the runtime libcef major version doesn't match the headers Pengu was compiled against:

```cpp
if (get_version == nullptr || get_version(0) != CEF_VERSION_MAJOR)
{
    if (is_browser)
        dialog::alert("Pengu does not support your Client version.", "Pengu Loader");
    return false;
}
```

Pengu currently targets **CEF 108** (commit `bf18f34`, headers under [`core/cef/`](../core/cef/)). Riot bumps LCUX's CEF major version roughly every two years, and each bump requires a Pengu release with new headers and possibly updated patterns (see next section).

### 2.5 Transparency hook (`CefContext::GetBackgroundColor`)

CEF, unlike Electron's patched Chromium, hardcodes white as the compositor root paint colour when alpha is zero. Setting `cef_browser_settings_t.background_color = 0` at create time isn't honoured end-to-end — the renderer still paints opaque white behind transparent UI, defeating any `WS_EX_LAYERED` / `NSVisualEffectView` work the loader does on the host window.

The fix in [`libcef.cc:11-25`](../core/src/libcef.cc#L11-L25) is a memory pattern scan inside libcef for the unexported `CefContext::GetBackgroundColor` function:

```cpp
#if OS_WIN
const char *pattern = "41 83 F8 01 74 0B 41 83 F8 02 75 0A 45 31 C0";
#elif OS_MAC
const char *pattern = "55 48 89 E5 83 FA 01 74 ?? 83 FA 02 75 ??";
#endif
```

Once located, the function is hooked to unconditionally return `0` (transparent). With the compositor returning transparent, the host window's vibrancy/Mica/acrylic effect actually shows through. See PenguLoader/PenguLoader#25 for the full investigation history.

The patterns are stable for the lifetime of a CEF major version (~2 years). Updating Pengu for a new CEF major requires re-deriving them by hand against the new `libcef.dll`/`libcef.dylib`.

### 2.6 Cache path

[`browser.cc`](../core/src/browser/browser.cc) overrides `cef_settings_t.cache_path` and `root_cache_path` in the `cef_initialize` hook, and `cef_request_context_settings_t.cache_path` in the `CreateContext` hook, pointing them at:

- Windows: `%LOCALAPPDATA%\Riot Games\League of Legends\Cache`
- macOS: `/Users/Shared/Riot Games/League Client/Cache`

**Why.** Up to and including 2024 / CEF 108, Riot didn't set any cache path on LCUX, so the client effectively ran in incognito mode — every session re-downloaded fonts, images, partner-iframe assets, etc. Pengu's override gives LCUX a real on-disk cache, making iframes and external resources noticeably faster.

Riot eventually started providing their own cache path (under `<LoL>/Saved/webcache`). The v1.2.0 `main` branch always overrides; the `refactor-2` branch contains a deferral fix (only set if Riot left the field empty), which will be ported back. Vanguard does not interact with cache files.

The `root_cache_path` set is required by CEF 108+, which warns/errors if it's missing (commit `d6aef96`).

---

## 3. Browser-process hooks

The browser process is the CEF process that owns windows, IO, and the CEF browser host objects. Pengu hooks four libcef exports here.

### 3.1 `cef_initialize` — settings and command line

[`browser.cc:204-235`](../core/src/browser/browser.cc#L204-L235):

- Wraps `app->on_before_command_line_processing` so we can shape the cmdline (next subsection).
- Sets `cache_path` and `root_cache_path` (see [§2.6](#26-cache-path)).

### 3.2 Command-line shaping — `OnBeforeCommandLineProcessing`

Riot bakes a number of switches into LCUX's command line. Pengu intercepts the callback to:

1. **Capture RCS credentials** for the `https://riotclient/` proxy if `use_riotclient` is enabled. The `--riotclient-app-port` and `--riotclient-auth-token` switches are stripped by CEF *after* this callback returns, so this is the only safe point to read them ([`browser.cc:138-142`](../core/src/browser/browser.cc#L138-L142)). The mechanism is pragmatic and acknowledged-fragile.
2. **Strip `--no-proxy-server`** if `use_proxy` is set, by rebuilding the entire command line via `init_from_string`.
3. **Append `--remote-debugging-port=<port>`** if `debug_port` is set in config (undocumented; see [§3.5](#35-devtools-built-in-and-remote)).
4. **Append `--disable-web-security`** if `isecure_mode` is set.
5. **Optimisation switches** (`optimized_client`, default on): `disable-background-timer-throttling`, `disable-backgrounding-occluded-windows`, `disable-renderer-backgrounding`, `disable-metrics`, `disable-component-update`, `disable-domain-reliability`, `disable-translate`, `disable-gpu-watchdog`, `disable-renderer-accessibility`, `no-sandbox`.
6. **Super-potato switches** if enabled: `disable-smooth-scrolling`, `wm-window-animations-disabled`, `animation-duration-scale=0`.

### 3.3 `cef_browser_host_create_browser` — main browser handshake

LCUX creates many CEF browsers in its lifetime: the main UX shell, the auth popup, embedded eSports streams, partner iframes hosted as separate browsers, etc. Plugins should only load into the main shell.

The detection in [`browser.cc:102-128`](../core/src/browser/browser.cc#L102-L128) checks the URL on `CreateBrowser`:

```cpp
if (url_.startw("https://riot:") && url_.endw("/bootstrap.html"))
{
    extra_info->set_null(extra_info, &u"is_main"_s);
    HookMainBrowserClient(client);
}
```

The `https://riot:` prefix matches the `riot:<token>@127.0.0.1:<port>` userinfo that RCS bakes into LCUX's URLs. The `bootstrap.html` filename is the very first page LCUX loads, which then redirects to `index.html` (the actual UI shell). This pattern has been stable across Client patches.

Stamping `is_main` into the browser's `extra_info` dictionary is how the *renderer* side knows which browser is the main one — `extra_info` is passed through to `OnBrowserCreated` in the renderer process via CEF's IPC. See [§4](#4-renderer-process-hooks).

For the main browser, [`HookMainBrowserClient`](../core/src/browser/browser.cc#L37-L100) wraps the client to:

- Hook `get_keyboard_handler` for hotkeys ([§3.6](#36-hotkeys)).
- Hook `get_life_span_handler` to wrap `on_after_created` and call `browser::setup_window` ([§3.7](#37-window-stack-and-transparency)).
- Hook `on_process_message_received` for the renderer-to-browser IPC channel ([§3.8](#38-renderer-to-browser-ipc)).

### 3.4 `cef_request_context_create_context` — custom schemes

When a request context is created, Pengu registers two scheme handler factories on it:

- **`https://plugins/`** — serves files out of the user's `plugins/` directory as ES modules. This is the heart of the plugin model and is documented in detail in [§6](#6-plugin-scheme-handler).
- **`https://riotclient/`** — opt-in proxy to the Riot Client's REST/WS API. See [§3.9](#39-riotclient-proxy).

Why the same `https` scheme rather than custom `pengu://`: the LCUX `index.html` historically had a CSP that only permitted `https://` origins (later relaxed to allow Riot partner iframes). Re-using `https` with a synthetic hostname avoids needing a CSP exemption and integrates cleanly with the rest of LCUX's web stack.

### 3.5 DevTools (built-in and remote)

DevTools is **off by default** in v1.2.0 (`use_devtools=false`). v1.1.6 had it on, v1.2.0 added a permission-gating model where users opt in.

**Built-in DevTools** is opened from JavaScript via `OpenDevTools()` (or hotkeys F12 / Ctrl+Shift+I). The renderer sends an `@open-devtools` IPC process message; the browser-side handler in [`devtools.cc`](../core/src/browser/devtools.cc) calls `host->show_dev_tools(host, &wi, new DevToolsClient(browser_id), &settings, nullptr)`.

The `DevToolsClient` argument is the key trick. League sets a non-1.0 zoom on the main browser host (e.g. 0.8× or 1.6× depending on user UI scale settings); if `show_dev_tools` is called with `nullptr` for the client, the DevTools window inherits that zoom and becomes unusable. Passing a fresh client decouples them. The custom client also installs a `DevToolsKeyboardHandler` for Ctrl±/Ctrl0 zoom (and Cmd-C/V/X fixes on macOS), and uses `window::get_scaling(window) - 1.0` as the initial zoom so DevTools defaults to 1.0× per-DPI.

`devtools_map_` dedupes by parent browser ID — a second `OpenDevTools()` call brings the existing window to the foreground rather than opening a duplicate.

**Remote DevTools** is enabled via the undocumented `debug_port` config option, which appends `--remote-debugging-port=<port>` to LCUX's command line. The user can then attach Chrome at `http://localhost:<port>/json`. It is undocumented because exposing remote control of the LCUX renderer over loopback is a real attack surface; advanced users can opt in by editing the config file directly.

### 3.6 Hotkeys

[`keyboard.cc`](../core/src/browser/keyboard.cc), gated on `use_hotkeys` (default on), main browser only:

| Key (Win) | Key (Mac) | Effect |
| --- | --- | --- |
| F12 / Ctrl+Shift+I | F12 / Cmd+Alt+I | Open DevTools (also gated on `use_devtools`) |
| Ctrl+Shift+R | Cmd+Alt+R | `browser->reload_ignore_cache` |
| Ctrl+Shift+Enter | Cmd+Alt+Enter | Confirm dialog → `fetch('/riotclient/kill-and-restart-ux')` for full restart |

All hotkeys are bypassed when focus is on an editable field, so they don't fight chat/search inputs.

### 3.7 Window stack and transparency

LCUX's window hierarchy on Windows ([`window.cc:53-66`](../core/src/browser/window.cc#L53-L66)):

```
RCLIENT                               <- Riot Client top-level window
  CefBrowserWindow                    <- default container by CreateBrowser()
    Chrome_WidgetWin_0                <- the actual Chromium widget
      Chrome_RenderWidgetHostHWND
      Intermediate D3D Window
```

Out of the box, `CefBrowserWindow` has its own opaque background, which would defeat any layered-window transparency on `RCLIENT`. [`browser::setup_window`](../core/src/browser/window.cc#L68-L113) hides `CefBrowserWindow` and re-parents `Chrome_WidgetWin_0` directly to `RCLIENT` so the compositor's transparent output paints straight onto the host window.

Combined with the [`GetBackgroundColor` hook](#25-transparency-hook-cefcontextgetbackgroundcolor) and the `apply_vibrancy` / `set_theme` calls (driven from JS via `Effect.apply`), this gives Electron-style Mica/acrylic/blur-behind on Windows and `NSVisualEffectView` material on macOS.

On macOS, no re-parenting is needed; `browser::window` is just the CEF host's `NSView*` and `NSVisualEffectView` does the heavy lifting natively.

#### Silent mode

If `silent_mode` is enabled, [`window.cc:96-110`](../core/src/browser/window.cc#L96-L110) hooks `ShowWindow`, `SetWindowPos`, and the window's `WndProc` to suppress LCU's own foreground/topmost calls. This is for users who don't want LCU flashing or popping itself topmost on matchmaking-found / postgame events. As a side-effect, the post-game lobby will not auto-show the client window.

### 3.8 Renderer-to-browser IPC

The browser-side `on_process_message_received` handler ([`browser.cc:56-99`](../core/src/browser/browser.cc#L56-L99)) understands four message names from the renderer:

| Message | Effect |
| --- | --- |
| `@open-devtools` | Calls `browser::open_devtools` (gated on `use_devtools`) |
| `@reload-client` | `browser->reload_ignore_cache` |
| `@set-window-vibrancy` | Calls `window::apply_vibrancy` or `clear_vibrancy` on the host window |
| `@set-window-theme` | Calls `window::set_theme(dark)` |

The renderer side sends these via `frame->send_process_message(PID_BROWSER, msg)` from the V8 helper functions in [`v8_helper.cc`](../core/src/renderer/v8_helper.cc).

### 3.9 `https://riotclient/` proxy

[`riotclient.cc`](../core/src/browser/riotclient.cc) registers an opt-in scheme handler that proxies `https://riotclient/<path>` to `https://127.0.0.1:<rcs_port>/<path>` with `Authorization: Basic base64("riot:<rcs_token>")` injected and `Access-Control-Allow-Origin: *` to let LCUX's web context call it.

LCUX already proxies many Riot endpoints under its own `/riotclient/...` path internally, so most plugins never need this. The custom scheme exists for the rare cases where a plugin needs an RCS endpoint LCUX doesn't re-expose (multi-product session info, account-switching, etc.).

It is **off by default** (`use_riotclient=false`), historically default-on but tightened in v1.2.0 alongside the rest of the permission model — handing every loaded plugin a Basic-auth-injected channel to RCS is risky if any plugin is malicious. The credential extraction described in [§3.2](#32-command-line-shaping--onbeforecommandlineprocessing) is fragile and is a known weak point.

---

## 4. Renderer-process hooks

The renderer process runs V8 and Blink. Pengu hooks the renderer-side `cef_execute_process` to attach to its `cef_render_process_handler_t`, then wires up two callbacks.

### 4.1 `OnBrowserCreated` — adopt the `is_main` flag

[`renderer.cc:259-268`](../core/src/renderer/renderer.cc#L259-L268):

```cpp
is_main_ = extra_info && extra_info->has_key(extra_info, &u"is_main"_s);
```

The `extra_info` dictionary is the same one stamped by the browser side in [§3.3](#33-cef_browser_host_create_browser--main-browser-handshake). When the renderer process is servicing the main shell's browser, `is_main_` becomes `true`.

> **Known issue.** `is_main_` is a per-process global reassigned on every `OnBrowserCreated`. If CEF ever reuses one renderer process for both a main and a non-main browser, the flag flickers. The narrowing URL check in `OnContextCreated` (next section) saves us in practice, but this is belt-and-suspenders, not a guarantee.

### 4.2 `OnContextCreated` — main-frame plugin injection

For each new V8 context, [`renderer.cc:215-242`](../core/src/renderer/renderer.cc#L215-L242):

1. Checks `is_main_ && url.startw("https://riot:") && url.endw("/index.html")`. Sub-browsers, popups, and the bootstrap step itself are skipped.
2. Calls three setup functions:
   - `ExposeOsObject` — `window.os = { name, version, build }` (per-platform values from [`platform.h`](../core/src/platform.h)).
   - `ExposeNativeFunctions` — `window.__native = { OpenDevTools, OpenPluginsFolder, ReloadClient, SetWindowTheme, SetWindowVibrancy, LoadDataStore, SaveDataStore }`. The preload reads these and then `delete window.__native` so plugins can't use them directly.
   - `LoadPlugins` — populates `window.Pengu = { version, plugins, disabledPlugins, superPotato, isMac }` where `plugins` is an array of plugin entry paths discovered on disk.
3. Runs the embedded preload script via `frame->execute_java_script`.

### 4.3 Plugin discovery

[`get_plugin_entries`](../core/src/renderer/renderer.cc#L15-L78) walks the configured plugins directory and recognises three layouts:

```
plugins/
  plugin-name.js              — top-level JS file
  plugin-name/
    index.js                  — plugin folder
  @author/
    plugin-name/
      index.js                — author-namespaced plugin
```

Names starting with `_` or `.` are skipped. Subfolders inside an `@author` folder are also subject to the underscore/dot skip. The list of relative paths is passed to JS via `Pengu.plugins`.

### 4.4 Native V8 handler

`__native` is exposed via a single shared `cef_v8handler_t` ([`renderer.cc:80-119`](../core/src/renderer/renderer.cc#L80-L119)) that dispatches by function name into a `std::unordered_map<std::string, V8FunctionHandler>` populated from two entry tables:

- [`v8_DataStoreEntries`](../core/src/renderer/v8_datastore.cc) — `LoadDataStore`, `SaveDataStore`.
- [`v8_HelperEntries`](../core/src/renderer/v8_helper.cc) — `OpenDevTools`, `OpenPluginsFolder`, `ReloadClient`, `SetWindowVibrancy`, `SetWindowTheme`.

The `OpenPluginsFolder` helper runs in-process (calls `shell::open_folder`); the others marshal to the browser process via process messages.

---

## 5. Preload bundle

[`packages/preload/`](../packages/preload/) is a TypeScript codebase compiled by Vite into a single IIFE that runs at the top of every main-frame V8 context. Its entry is [`src/index.ts`](../packages/preload/src/index.ts), which imports two trees:

- `./preload/` — the runtime bootstrap (DataStore, Effect, RCP/socket, plugin loader).
- `./views/` — the in-client overlay UI (Solid components for plugin toggles, etc.).

### 5.1 Build pipeline

The Vite config in [`packages/preload/vite.config.ts`](../packages/preload/vite.config.ts) has two modes:

**Dev (`vite dev`):** Vite serves the bundle on `https://localhost:3001`. esbuild emits `dist/preload.js` (IIFE) with a footer that imports `@vite/client` and `src/views/index.tsx` from the dev server, so HMR works inside LCUX. The C++ side, when `_DEBUG`, reads `dist/preload.js` from disk at runtime ([`renderer.cc:200`](../core/src/renderer/renderer.cc#L200)) and `execute_java_script`s it.

**Build (`vite build`):** Vite emits `dist/preload.js`, then a custom `pengu-build` plugin emits `dist/preload.g.h` containing `_preload_script[]` as a C `unsigned char` array. The renderer `#include`s that header in non-debug builds and runs the embedded bytes ([`renderer.cc:209-211`](../core/src/renderer/renderer.cc#L209-L211)).

> **Known issue.** The recent `7635b4c` refactor moved `plugins/` → `packages/preload/`, but the C++ paths in `renderer.cc` still reference `../plugins/dist/preload.js` and `../../plugins/dist/preload.g.h`, which no longer resolve. Release builds will fail until either the C++ paths are updated or the build emits to `core/plugins/dist/`. To be fixed.

### 5.2 Plugin loader

[`preload/loader.ts`](../packages/preload/src/preload/loader.ts) is the entry point that turns `Pengu.plugins` into actually loaded modules:

1. **Filter disabled plugins.** `Pengu.disabledPlugins` arrives as a comma-separated string of FNV-1a 32-bit hashes (lowercased, forward-slash-normalised paths). The loader parses it into a `Set<number>`, hashes each entry, and filters out matches. Any entry matching `/^@default\//i` is also dropped — `@default/` was the v0.6 built-in plugin namespace from the old "League Loader" branding and is vestigial.
2. **Dynamic-import each entry.** `await import('https://plugins/' + entry)`. This goes through the [`https://plugins/` scheme handler](#6-plugin-scheme-handler).
3. **Init.** If the module exports an `init` function, await it with `{ rcp, socket, meta }`. `meta.name` is the plugin's folder name (omitted for top-level `name.js` plugins).
4. **Load.** If the module exports `load` or `default` as a function, register it as a `window` `'load'` listener.

The whole batch is wrapped in `Promise.all`, then `rcp.preInit('rcp-fe-common-libs', () => waitable)` blocks the first RCP plugin until plugin loading finishes, ensuring plugins see a consistent initial state.

> **Known issue.** The hash-based disable mechanism is essentially untested in v1.2.0. v1.1.6 disables plugins by renaming `index.js` → `index.js_` on disk; the Tauri loader has partial backward-compat for that ([`packages/hub/src/lib/plugins.ts`](../packages/hub/src/lib/plugins.ts) `isIndex` checks `path += '_'`), but the end-to-end flow has not been validated.

### 5.3 Public surface exposed to plugins

After the preload runs, plugins see:

- **`window.Pengu`** — `{ version, plugins, superPotato, isMac }` (frozen). `disabledPlugins` is consumed by the loader and removed.
- **`window.os`** — `{ name, version, build }`.
- **`window.DataStore`** — `{ has, get, set, remove }` for shared key-value storage.
- **`window.Effect`** — `{ apply, clear, setTheme }` for window vibrancy/theme. Translates between Windows backdrop types (`transparent`, `blurbehind`, `acrylic`, `unified`, `mica` with sub-modes `auto/none/mica/acrylic/tabbed`) and macOS `NSVisualEffectMaterial` values.
- **`window.rcp`** — `{ preInit, postInit, whenReady, get }`. Wraps Riot's plugin announce/registration mechanism so plugins can intercept other plugins' init.
- **`window.rcp` is a `RCP` class instance**, but the global `rcp` and `socket` are also passed as `init` arguments to plugins.
- **`window.openDevTools`**, **`openPluginsFolder`**, **`reloadClient`**, **`restartClient`**, **`getScriptPath`** — convenience helpers.

`window.__native` is deleted from the global scope by [`api/native.ts`](../packages/preload/src/preload/api/native.ts) immediately after capture, so plugins must go through the typed wrappers.

### 5.4 RCP hooks

[`preload/rcp/hooks.ts`](../packages/preload/src/preload/rcp/hooks.ts) wraps `document.dispatchEvent` to intercept Riot's `riotPlugin.announce:<name>` events. Each announce carries a `registrationHandler` that gets wrapped so before/after callbacks registered via `rcp.preInit(name, fn)` and `rcp.postInit(name, fn)` fire at the right lifecycle points (before the registrar runs, after the registered API is fulfilled). `whenReady(name | names[])` returns a promise resolving to the plugin's API once it's fulfilled.

This is the canonical way for plugins to extend or wrap LCUX's own "rcp-fe-*" frontend plugins.

### 5.5 WAMP socket helper

[`preload/rcp/socket.ts`](../packages/preload/src/preload/rcp/socket.ts) opens a WAMP WebSocket to LCUX's internal LCDS endpoint (URL provided by `rcp-fe-common-libs.context.socket._endpoint`) and exposes `socket.observe(api, listener)` / `disconnect(api, listener)`. The api string is normalised into LCDS's `OnJsonApiEvent_<path_with_underscores>` topic format (or `OnJsonApiEvent` when api is `'all'`).

This is the standard way for plugins to subscribe to live LCU events (queue state, gameflow, summoner, etc.).

### 5.6 Late-listener polyfills

[`preload/load-hooks.ts`](../packages/preload/src/preload/load-hooks.ts) wraps `window.addEventListener` and `document.addEventListener` so that plugins registering for `load` / `DOMContentLoaded` *after* those events have already fired still get their listener invoked (via `setTimeout(listener, 1)`). Without this, plugins that bundle slowly enough to register past the `load` event would silently fail.

### 5.7 Super-potato mode

[`preload/super-potato.ts`](../packages/preload/src/preload/super-potato.ts), enabled when `Pengu.superPotato` is true:

- Injects a global stylesheet that disables CSS transitions everywhere except a curated set of essential animations (spinners, vignette celebrations).
- Wraps `document.createElement` to inject the same stylesheet into any new shadow root.
- Calls Riot's `lol-settings/v1/local/lol-user-experience` endpoint to enable the in-game potato mode.

Combined with the cmdline switches added by `optimized_client` and the super-potato cmdline switches in [§3.2](#32-command-line-shaping--onbeforecommandlineprocessing), this shaves significant CPU on low-end machines.

---

## 6. Plugin scheme handler

The `https://plugins/` scheme handler ([`assets.cc`](../core/src/browser/assets.cc)) is the heart of the plugin model. It serves files out of the configured `plugins/` directory as ES modules, with on-the-fly module shims that mimic Vite's import semantics.

### 6.1 Path resolution

For a request to `https://plugins/<path>?<query>`:

1. URL-decode the path.
2. Resolve to `<plugins_dir>/<path>`.
3. If the path ends in `/`, append `index.js`.
4. If the leaf has no extension, try `<path>.js`, then `<path>/index.js`.
5. If none of those exist, return 404.

### 6.2 Module shims

If the request `resource_type` is `RT_SCRIPT` (i.e. an `import` or `<script type="module">`), the handler picks one of these synthesized module sources based on the file extension and query string:

| Match | Source served | Effect on `import` |
| --- | --- | --- |
| `?url` | `SCRIPT_IMPORT_URL` | `export default <url-with-query-stripped>` |
| `?raw` | `SCRIPT_IMPORT_RAW` | `export default await fetch(url).then(r => r.text())` |
| `.css` | `SCRIPT_IMPORT_CSS` | Appends `<link rel="stylesheet" href="<url>">` to body, exports nothing |
| `.json` | `SCRIPT_IMPORT_JSON` | `export default JSON.parse(await fetch(url).then(r => r.text()))` |
| Image / media / font (png, jpg, svg, mp4, woff2, ttf, …) | `SCRIPT_IMPORT_URL` | Same as `?url` — `import logo from './logo.png'` gives a URL string |
| Anything else | Raw file bytes with MIME from `cef_get_mime_type` | Normal JS module |

These shims are intentionally Vite-shaped so plugin authors used to Vite dev mode can write plain JS+CSS plugins without a bundler. Plugin authors using a bundler (esbuild, Rollup, etc.) typically don't need them and just import bundled output.

### 6.3 Caching

- **JavaScript modules** (any path served as `text/javascript`) get `Cache-Control: no-store`.
- **All other assets** get `Cache-Control: max-age=31536000, immutable` plus an `ETag` derived from FNV-1a of the URL.

The reason for `no-store` on JS specifically: LCUX's renderer URLs are stable across in-session reloads (Ctrl+Shift+R, custom DevTools reload). If JS were cached, the cache would survive a reload — and since the URL doesn't change, plugin authors couldn't iterate without restarting the entire client (which Riot's random-port handoff makes slow). Forcing fresh JS on every fetch lets reload do what users expect.

Assets stay aggressively cached because they're heavy (textures, fonts, audio) and their content rarely changes; the ETag handles the rare update.

### 6.4 Range requests

Audio/video plugins occasionally need range support (e.g. seeking a long mp4). The handler parses `Range: bytes=<start>-<end>` headers and returns `206 Partial Content` with `Accept-Ranges`, `Content-Range`, and `Content-Length` set. Malformed ranges return `416 Requested Range Not Satisfiable`.

### 6.5 CORS

Every response sets `Access-Control-Allow-Origin: *`. This is needed because plugins are served from `https://plugins/` but consumed from the LCUX origin `https://127.0.0.1:<port>/`, and `fetch` from a plugin's own JS would otherwise hit CORS.

---

## 7. Loader

The loader is a standalone GUI app responsible for installing/uninstalling the core, managing plugins, and (on macOS) watching for LCUX launches. It does **not** run inside LCUX.

### 7.1 Today: Tauri (`packages/hub/`)

The current loader is a Tauri app:

- **Frontend** ([`packages/hub/src/`](../packages/hub/src/)) — SolidJS, Tailwind, Radix-style primitives. Pages: Splash, Welcome, Main (plugins, settings, store).
- **Backend** ([`packages/hub/src-tauri/`](../packages/hub/src-tauri/)) — Rust with platform-split modules.

Backend responsibilities:

- **Config** ([`config.rs`](../packages/hub/src-tauri/src/config.rs)) — locates `core.dll`/`core.dylib` and `config` next to itself.
- **Windows** ([`windows/`](../packages/hub/src-tauri/src/windows/)) — `mod_ifeo.rs` and `mod_symlink.rs` activation, WebView2 presence check, window shadow setup. UAC elevation via `runas` when admin is needed for IFEO writes.
- **macOS** ([`macos/`](../packages/hub/src-tauri/src/macos/)) — RCS WAMP socket daemon, `insert_dylib` (Rust port + external binary fallback), system tray, startup-on-login, traffic-light hiding.
- **Shell** ([`shell.rs`](../packages/hub/src-tauri/src/shell.rs)) — `expand_folder`, `reveal_file` for opening Explorer/Finder.

The frontend invokes these Rust commands via `@tauri-apps/api`. Plugin metadata is parsed from JSDoc tags (`@description`, `@author`, `@link`) inside each plugin's entry file. The plugin store ([`store.ts`](../packages/hub/src/lib/store.ts)) fetches a YAML registry from `github.com/PenguLoader/plugin-store` — currently a placeholder with no plan to implement install automation.

### 7.2 Future: `app/` (.NET 10)

The Tauri implementation is planned to be replaced by a from-scratch .NET 10 host:

- **Windows** — WebView2 hosting the SolidJS UI.
- **macOS** — WKWebView hosting the SolidJS UI.

`packages/hub/` will then contain only the SolidJS frontend assets; the host process and IPC (replacing Tauri's `invoke`) move into `app/`. The activation, plugin management, and macOS RCS-watcher logic carry over from the Rust backend in concept but get rewritten in C#.

The `refactor-2` branch contains an older C# experiment that embedded the hub UI directly into the Riot Client window via DevTools Protocol attachment. It was deemed too complex and is not the basis for `app/`.

### 7.3 Production: v1.1.6

Until `app/` is ready, the actually shipped loader is **v1.1.6**, a Windows-only WPF / .NET Framework app. It uses IFEO activation, disables plugins by renaming `index.js` → `index.js_` on disk, and does not support macOS at all. The v1.2.0 codebase on `main` is not user-released.

---

## 8. Configuration

The config file lives next to the loader binary as `config` (no extension). It's a plain `key = value` ini-style file with `;` and `#` for comments. Unknown keys are ignored; `loader_dir / "config"` is read at every option lookup but cached after the first miss/hit.

| Key | Type | Default | Description |
| --- | --- | --- | --- |
| `plugins_dir` | path | `<loader_dir>/plugins` | Override plugins folder location |
| `disabled_plugins` | csv hex | `""` | Comma-separated FNV-1a 32-bit hex hashes of disabled plugin paths |
| `use_hotkeys` | bool | `true` | Enable F12 / Ctrl+Shift+I / Ctrl+Shift+R / Ctrl+Shift+Enter |
| `optimized_client` | bool | `true` | Append performance-related Chromium switches |
| `super_potato` | bool | `false` | Disable animations + Riot's potato mode |
| `silent_mode` | bool | `false` | Suppress LCU's foreground/topmost calls |
| `isecure_mode` | bool | `false` | Append `--disable-web-security` |
| `use_devtools` | bool | `false` | Allow `OpenDevTools()` and DevTools hotkeys |
| `use_riotclient` | bool | `false` | Register `https://riotclient/` proxy scheme |
| `use_proxy` | bool | `false` | Strip `--no-proxy-server` so HTTP proxy env vars are honoured |
| `debug_port` | int | `0` | Append `--remote-debugging-port=<port>` (undocumented) |
| `league_dir` | path | `""` | Used by the loader (symlink mode) to locate the LoL install |

---

## 9. Persistence

`window.DataStore` reads and writes a single shared file at `<loader_dir>/datastore`. Format on disk: JSON, XOR'd byte-by-byte against a cycling 20-byte key (`A5dgY6lz9fpG9kGNiH1mZ`).

The XOR is **not encryption** — the key is a string literal in the binary, anyone can recover plaintext in seconds. It exists to keep casual users from opening the file in a text editor and corrupting it. A speed bump, not a security boundary.

Plugins share a single namespace and are expected to prefix their own keys to avoid collisions; the API does not enforce per-plugin scoping today. There is no quota.

`localStorage` and `IndexedDB` are not viable cross-launch options because LCUX's origin is `https://127.0.0.1:<port>/` and the port changes on every Riot Client launch — browser storage scoped to that origin gets orphaned. `DataStore` is the only durable cross-launch storage available to plugins.

A future iteration is planned to replace `DataStore` with per-plugin async storage.

---

## 10. Known issues / TODO

Surfaced separately so they don't get lost in implementation prose:

1. **Preload include path is broken on `main`.** Commit `7635b4c` moved `plugins/` to `packages/preload/`. The C++ references in [`renderer.cc:200,209`](../core/src/renderer/renderer.cc#L200) still point at the old path. Release builds will fail until either the C++ side is updated or the build emits to `core/plugins/dist/`.
2. **`is_main_` flicker.** Per-process global reassigned on every `OnBrowserCreated` ([`renderer.cc:265`](../core/src/renderer/renderer.cc#L265)). Currently saved by the `index.html` URL check downstream, but not robust if CEF's process model ever changes.
3. **Hash-based disable is essentially untested.** v1.1.6 uses on-disk renames; v1.2.0 uses FNV-1a hashes. The new path has not been validated end-to-end. Tauri loader has partial backward-compat for `.js_`-renamed plugins ([`plugins.ts:131`](../packages/hub/src/lib/plugins.ts#L131)) but the entry path produced for those would not satisfy the C++ scheme handler.
4. **RCS credential extraction is fragile.** The `OnBeforeCommandLineProcessing` window is the only known point to read `--riotclient-app-port` / `--riotclient-auth-token` before CEF strips them. There is no clean alternative.
5. **`OnDemand` enum on Windows is dead.** Defined in the loader UI ([`core-module.ts:5-9`](../packages/hub/src/lib/core-module.ts#L5-L9)) but only `Universal` (IFEO) and `Targeted` (symlink) are wired to Rust. macOS implicitly is OnDemand and ignores the enum.
6. **Cache path always overrides on `main`.** `refactor-2` contains a fix to defer to Riot's own `Saved/webcache` when set. Should be ported back.

---

## 11. References

- v0.6.0 [`HOW_IT_WORKS.md`](https://github.com/PenguLoader/PenguLoader/blob/v0.6.0/HOW_IT_WORKS.md) — original architecture write-up. Many concepts (CommonJS `require`, d3d9 proxying, CreateRemoteThread DevTools) are now superseded.
- [PenguLoader/PenguLoader#25](https://github.com/PenguLoader/PenguLoader/issues/25) — full investigation history of the CEF transparency hook.
- v1.1.6 — production codebase (WPF / .NET Framework), tag [`v1.1.6`](https://github.com/PenguLoader/PenguLoader/tree/v1.1.6).
- `refactor-2` branch — discarded experiment that embedded the hub UI inside LCUX via DevTools Protocol. Not the basis for the `app/` rewrite.
- [Mecha](https://github.com/x00bence/Mecha) — the original IFEO+Detours injector that inspired the v0.6 era.
