# `app/` — Pengu host (.NET 10) — Design

> Status: design, pre-implementation. Replaces [`packages/hub/src-tauri/`](../packages/hub/src-tauri/). Companion doc to [`docs/design.md`](./design.md), which covers the full v1.2.0 architecture (core + preload + hub).

This document covers the from-scratch .NET 10 host that takes over the loader UI and platform plumbing role from the Tauri+Rust implementation. The SolidJS UI in [`packages/hub/src/`](../packages/hub/src/) and the C++ core in [`core/`](../core/) carry over; only the host process and IPC layer is rewritten in C#.

---

## 1. Goals and scope

**In scope:**
- Replace `packages/hub/src-tauri/` with a from-scratch .NET 10 host.
- Cross-platform: WebView2 on Windows, WKWebView on macOS. Both NativeAOT.
- Reuse the existing SolidJS UI (with bridge calls migrated from `@tauri-apps/api`).
- Mirror the activation, plugin discovery, daemon (macOS only), and tray (macOS only) behaviors of the Rust loader, with the redesigns called out below.

**Out of scope:**
- A cross-platform native dialog framework (overlays in the hub UI replace `dialog.message` etc.).
- Plugin store install automation (the YAML registry stays a placeholder).
- Crash dump / minidump generation.
- Portable-install mode (drop `app.exe` and have it not touch `%PROGRAMDATA%`).
- A generic shell/fs escape hatch (the bridge surface is narrow by design).

---

## 2. Project layout

Four csproj files live under [`app/`](../app/), all added to the root [`pengu.slnx`](../pengu.slnx):

```
app/
  Directory.Build.props        shared MSBuild props (LangVersion, Nullable, AOT, ...)
  Pengu/                       net10.0 — platform-neutral
    Pengu.csproj
  Pengu.Windows/               net10.0-windows — Windows head, outputs Pengu.exe
    Pengu.Windows.csproj
  Pengu.MacOS/                 net10.0-macos — macOS head, outputs Pengu.app/Contents/MacOS/Pengu
    Pengu.MacOS.csproj
  Pengu.Gen/                   netstandard2.0 — incremental source generator
    Pengu.Gen.csproj
```

`Directory.Build.props` carries everything common: `<LangVersion>latest</LangVersion>`, `<Nullable>enable</Nullable>`, `<ImplicitUsings>enable</ImplicitUsings>`, `<AllowUnsafeBlocks>true</AllowUnsafeBlocks>`, `<PublishAot>true</PublishAot>`, `<InvariantGlobalization>true</InvariantGlobalization>`, `<IlcOptimizationPreference>Size</IlcOptimizationPreference>`, `<TrimMode>full</TrimMode>`. Each csproj only declares what's project-specific (TargetFramework, OutputType, project-specific `<PackageReference>`).

**Why split the heads instead of multi-targeting one project?** The platform asymmetry is roughly 50/50 (each head ~1500–2500 LOC of platform-specific code; ~1000 LOC genuinely shared). Splitting:
- Compile-time isolation of platform dependencies (`Vanara.PInvoke.*`, `Diga.WebView2.Interop.AOT` on Windows; macOS workload bindings on macOS) without bracketing every `<PackageReference Condition="...">`.
- Halves AOT publish time when iterating on one platform.
- `dotnet publish app/Pengu.Windows -c Release -r win-x64` is unambiguous about what it produces.
- Pengu.Core compiles once for plain `net10.0`; both heads consume the same DLL — drift between platform views of "shared" code is a type error.

**Pengu.Gen** is the incremental source generator that emits the JS-bridge dispatcher for every `[JsInterop("name")] partial class`. Each head pulls it via `<ProjectReference Include="..\Pengu.Gen\Pengu.Gen.csproj" OutputItemType="Analyzer" ReferenceOutputAssembly="false" />`.

---

## 3. Build pipeline

### 3.1 Orchestration

A single `dotnet publish` builds everything. An MSBuild target in each head's csproj invokes pnpm and zips the hub bundle, **gated on `'$(Configuration)' == 'Release'`**:

```xml
<Target Name="BuildHubBundle" BeforeTargets="BeforeBuild"
        Condition="'$(Configuration)' == 'Release'">
  <Exec Command="pnpm --filter @pengu/hub build"
        WorkingDirectory="$(MSBuildThisFileDirectory)..\.."/>
  <ZipDirectory SourceDirectory="$(MSBuildThisFileDirectory)..\..\packages\hub\dist"
                DestinationFile="$(IntermediateOutputPath)app.dat"
                Overwrite="true"/>
</Target>

<ItemGroup Condition="'$(Configuration)' == 'Release'">
  <None Include="$(IntermediateOutputPath)app.dat"
        CopyToOutputDirectory="PreserveNewest"/>
</ItemGroup>
```

Debug builds skip the zip — the host runs in `--dev=<vite-url>` mode and never reads `app.dat`.

### 3.2 Dev workflow

```
pnpm --filter @pengu/hub dev          # serves http://localhost:1420 with HMR
dotnet run --project app/Pengu.Windows -- --dev=http://localhost:1420
```

`Pengu.Windows`'s `Properties/launchSettings.json` defaults the `--dev=...` arg so `dotnet run` works without it.

### 3.3 Release output

```
<install_dir>/                  # the user-facing folder
  Pengu.exe                     # AOT-published, ~12 MB
  app.dat                       # zipped SolidJS hub, ~200–400 KB
  core.dll                      # C++ core (Windows)
```

Other state (`config`, `datastore`, `plugins/`) lives under `%PROGRAMDATA%\.pengu\` (machine-wide, ACL'd to allow all users); WebView2 cache lives separately under `%LOCALAPPDATA%\Pengu\WebView2\` (per-user). See [§11](#11-data-layout). macOS analog: `Pengu.app/Contents/MacOS/Pengu` + `Pengu.app/Contents/Resources/{app.dat,core.dylib}` packaged via post-publish MSBuild target.

---

## 4. Host runtime

### 4.1 Dispatcher

A custom message-loop dispatcher modeled on Avalonia's `Win32DispatcherImpl`:

- Drains a `ConcurrentQueue<Action>` of posted work.
- `PeekMessage` drains pending Win32 messages.
- `MsgWaitForMultipleObjectsEx` blocks until either a new message arrives or a wakeup `Event` is signalled.

`Dispatcher.UIThread.Post(Action)` from any thread; `InvokeAsync(Action) → Task` for awaitable cross-thread dispatch. Installed as the `SynchronizationContext` so `await` continuations resume on the UI thread by default. macOS uses `NSRunLoop` with the same `Dispatcher.UIThread` API surface; platform impls in `Platforms/`.

This avoids WPF/WinForms entirely — both pull large dependency trees and have AOT/trimming caveats.

### 4.2 Process boundaries

The Windows host wires itself into a Win32 Job Object with `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE` so any WebView2 children die with the host even on hard crash. Without this, a stuck WebView2 process can hold the user-data-folder lock and produce `ERROR_BUSY` on the next launch. macOS has no equivalent need (WebKit's helper processes already follow the parent).

### 4.3 Single-instance lock

```csharp
using var mutex = new Mutex(true, "989d2110-46da-4c8d-84c1-c4a42e43c424", out var createdNew);
if (!createdNew)
{
    // signal the running instance to focus its window, then exit
    PostMessage((IntPtr)0xFFFF, WM_SHOWME, IntPtr.Zero, IntPtr.Zero);
    return 0;
}
```

`WM_SHOWME = RegisterWindowMessage("Pengu Loader")` returns a system-unique message ID. The first instance's BorderlessWindow `WndProc` handles it by `SW_RESTORE` + `SetForegroundWindow`. Same UUID and the same broadcast pattern as v1.1.6 — cross-version compatibility during the migration window means new and old binaries treat each other as "already running."

macOS has no broadcast-message equivalent. Use `CFNotificationCenterPostNotification` on the darwin notification center with name `com.pengu.lol.show`; first instance subscribes to it and brings its window forward. ~30 lines of `LibraryImport`.

---

## 5. Browser host

### 5.1 Windows: Diga.WebView2.Interop.AOT

We use the full COM-source-generated WebView2 binding (`Diga.WebView2.Interop.AOT`) plus a raw `[LibraryImport]` of `WebView2Loader.dll`. **Not** the official `Microsoft.Web.WebView2` package — its public surface assumes WPF/WinForms host and isn't AOT-clean.

`WebView2Environment` is a process-wide singleton initialized once at startup, before any window opens. It registers the custom `app://` scheme via `ICoreWebView2CustomSchemeRegistration` (must happen at env-init time; later registrations are silently ignored). Additional CLI feature: `--enable-features=msWebView2EnableDraggableRegions` so the hub's HTML titlebar can use `app-region: drag` CSS for window dragging — no explicit `host.startDragging()` call needed for the common case.

WebView2 user-data folder: `%LOCALAPPDATA%\Pengu\WebView2\` (per-user, separate from the machine-wide `%PROGRAMDATA%\.pengu\` data root since browser cache must not be shared across users). The hub doesn't use cookies/storage in practice but WebView2 requires the folder to exist.

### 5.2 macOS: WKWebView via .NET 10 macOS workload

`net10.0-macos` ships AppKit/WebKit bindings in `Microsoft.macOS.Sdk` — modern, official, AOT-supported. Used directly; no third-party wrapper. Browser host is a thin C# class wrapping `WKWebView`, `WKScriptMessageHandler` (for the bridge), and `WKURLSchemeHandler` (for `app://`).

### 5.3 WebView2 missing on Windows

If `WebView2Loader.GetAvailableCoreWebView2BrowserVersionString` returns failure, show a `TaskDialog` (Vista-style, modal) with body "WebView2 is not installed on your system." and a clickable `<a href="https://developer.microsoft.com/.../webview2/">Install WebView2</a>` link, then exit. **The Evergreen Bootstrapper is not shipped** alongside `Pengu.exe` — keeps the install footprint lean; the dialog points users at the official installer.

### 5.4 `app://hub/` scheme handler

For the production payload, the hub bundle is served from a zip file at `<exe_dir>/app.dat` via a `WebResourceRequested` filter on `app://*` (Windows) / `WKURLSchemeHandler` (macOS). Path resolution:

```
app://hub/                   →  index.html
app://hub/assets/foo.css     →  assets/foo.css
```

Implementation is a `Dictionary<string, ZipArchiveEntry>` built once when the zip is first opened, then per-request: read entry bytes, wrap in a `MemoryComStream` (Win) / `NSData` (Mac), set `Content-Type` from a small mime-type table, return as `ICoreWebView2WebResourceResponse` / `WKURLSchemeTask`.

In dev mode (`--dev=<url>`), the scheme handler is not registered; the host navigates directly to the Vite dev server.

---

## 6. Window

A minimal borderless window, no GDI custom chrome — the SolidJS UI paints its own titlebar in HTML.

**Win32 (`Pengu.Windows/Window/BorderlessWindow.cs`):**
- `WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN`, no `WS_THICKFRAME` (hub is non-resizable today).
- `WM_NCCALCSIZE` returns `IntPtr.Zero` to remove the standard non-client area; client rect fills the entire window.
- `WM_NCHITTEST` returns `HTCLIENT` for the whole window. Drag is via the page's `app-region: drag` CSS; resize edges aren't exposed (non-resizable).
- DWM: `DwmSetWindowAttribute(DWMWA_USE_IMMERSIVE_DARK_MODE, true)` for dark titlebar (vestigial since titlebar is hidden, but cleans up the 1-pixel edge), `DwmExtendFrameIntoClientArea` with `MARGINS{top=1}` for native shadow, `DwmSetWindowAttribute(DWMWA_WINDOW_CORNER_PREFERENCE, DWMWCP_ROUND)` on Win11.
- `WM_DPICHANGED` resizes per OS suggestion; the hub re-renders against the new DPI naturally via CSS.

**macOS (`Pengu.MacOS/Window/BorderlessWindow.cs`):**
- `NSWindow` with `styleMask` = `.resizable | .closable | .miniaturizable | .fullSizeContentView`, **without** `.titled`.
- `titlebarAppearsTransparent = true`, `titleVisibility = .hidden`, traffic lights hidden via `[standardWindowButton:setHidden:]` for close/min/zoom.
- `backgroundColor = NSColor.clear` so the SolidJS background paints through.

The `IHost.GetMainWindow()` abstraction in `Pengu` is just a contract-bearing interface — heads instantiate their concrete window class and pass it through.

---

## 7. Bridge (IPC)

The bridge is a custom typed RPC over WebView2's `chrome.webview.postMessage` (Win) / `WKScriptMessageHandler` (Mac). It replaces every `@tauri-apps/api` call.

### 7.1 Wire format

**Request** (renderer → host):
```json
{"id": 42, "channel": "activation.setActive", "args": [true]}
```

**Reply** (host → renderer):
```json
{"id": 42, "ok": true, "result": {"ok": true}}
{"id": 42, "ok": false, "error": "elevation rejected"}
```

**Push event** (host → renderer, no id):
```json
{"event": "activation:stateChanged", "active": true}
```

The JS shim translates pushes into `window.dispatchEvent(new CustomEvent("activation:stateChanged", { detail }))`.

### 7.2 C# side: source generator

```csharp
[JsInterop("activation")]
internal partial class ActivationApi
{
    [JsInvokable] public Task<ActivationMode> GetMode() { ... }
    [JsInvokable] public Task<bool> IsActive() { ... }
    [JsInvokable] public Task<ActivationResult> SetActive(bool active) { ... }
}
```

`Pengu.Gen.JsInteropGenerator` emits a partial that implements `IJsInteropDispatcher` with a switch on the method name, deserializes args via `PenguJsonContext` (a `JsonSerializerContext` source-gen for AOT cleanliness), invokes the method, serializes the result, and returns it as a JSON string. All bridge methods are `async` (Task or Task<T>).

Per-window `JsBridge` instance owns the `Dictionary<string, IJsInteropDispatcher>` keyed by global name (`"activation"`, `"config"`, etc.), wires the `WebMessageReceived` handler, and provides `EmitEvent(name, payload)` for C#-originated pushes. The shim is injected via `AddScriptToExecuteOnDocumentCreated` once before the first navigation.

### 7.3 JS side: `window.pengu.*` rolled-up facade

```js
// Generated shim (template)
window.pengu = {
  activation: makeProxy("activation"),
  config:     makeProxy("config"),
  plugins:    makeProxy("plugins"),
  league:     makeProxy("league"),
  host:       makeProxy("host"),
  i18n:       makeProxy("i18n"),
  fs:         makeProxy("fs"),
  path:       makeProxy("path"),
};
```

`makeProxy` returns a `Proxy` that turns property access + call into a `{id, channel: "name.method", args}` postMessage and returns a Promise resolving on the matching reply. The hub imports a typed `lib/pengu.ts` facade for autocomplete; calls look like:

```ts
const ok = await pengu.activation.isActive();
await pengu.config.write({ app: { language: 'en' } });
```

C# → JS pushes use `window.addEventListener('activation:stateChanged', e => ...)`.

---

## 8. API surface

### 8.1 `pengu.activation`

```
activation.listModes()              ActivationModeInfo[]   [{mode, available, requiresAdmin}]
activation.getMode()                ActivationMode         current mode from config
activation.setMode(mode)            void                   updates config
activation.isActive()               bool                   per current mode
activation.setActive(active)        ActivationResult       {ok, error?, stage?}
activation.coreExists()             bool                   core.dll/dylib resolvable
```

`ActivationMode` enum: `Universal = 0`, `OnDemand = 2`. The `Targeted = 1` enum value is reserved (unused) so future config files written by old hubs that picked Targeted don't break — they fall back to Universal at read time.

`ActivationResult` typed object replaces today's "non-empty error string means failure" string. Stage codes (`OpenIFEO`, `SetDebugger`, etc.) become string enums, not bit-packed integers.

### 8.2 `pengu.config`

```
config.getRoot()                    string                 e.g. "C:\Users\...\AppData\Local\.pengu"
config.getPath()                    string                 path to the config file
config.read()                       ConfigSnapshot         { app: {...}, client: {...} } typed
config.write(patch)                 void                   partial merge, atomic flush
```

`ConfigSnapshot` is a typed C# record with the same shape as today's `defaultConfig` in [`packages/hub/src/lib/config.ts`](../packages/hub/src/lib/config.ts):

```csharp
public record ConfigSnapshot(ConfigApp app, ConfigClient client);
public record ConfigApp(string language, string plugins_dir,
                        string disabled_plugins, ActivationMode activation_mode);
public record ConfigClient(bool use_hotkeys, bool optimized_client, bool silent_mode,
                           bool super_potato, bool insecure_mode, bool use_devtools,
                           bool use_riotclient, bool use_proxy);
```

Ini parsing/writing moves to C# (`Pengu.Config.IniReader/IniWriter`). Atomic write: write to `<path>.tmp`, fsync, rename — eliminates a class of corruption bugs the current `writeTextFile` is exposed to.

### 8.3 `pengu.plugins`

```
plugins.list()                      PluginInfo[]           discovery + JSDoc parse + disabled flag
plugins.toggleEnabled(path)         bool                   new state
plugins.openFolder()                void
plugins.revealInFolder(path)        void
plugins.fetchStoreRegistry()        StorePlugin[]          GitHub YAML registry
```

`PluginInfo` carries `{ name, path, hash, author?, description?, link?, enabled }`. Discovery walks the plugins directory (top-level `.js`, `name/index.js`, `@author/name/index.js`), parses `@description` / `@author` / `@link` tags from each entry file via regex, computes the FNV-1a 32-bit hash, and intersects with the `disabled_plugins` config csv. ~80 LOC C#.

Store registry is a YAML fetch from `https://raw.githack.com/PenguLoader/plugin-store/main/registry/plugins.yml` — placeholder for now per [§10 of design.md](./design.md#10-known-issues--todo).

### 8.4 `pengu.league` (Windows)

```
league.findInstall()                string?                walks RiotClientInstalls.json
league.validateDir(dir)             bool                   exists(dir/LeagueClientUx.exe)
```

Reads `C:\ProgramData\Riot Games\RiotClientInstalls.json`, walks `rc_live` / `rc_default` / `associated_client`. ~30 LOC. macOS implementation returns `null` for `findInstall` (no equivalent path); the hub UI uses different copy on macOS.

### 8.5 `pengu.host`

```
host.getInfo()                      HostInfo               { os, version, build, isMac, isAdmin, locale }
host.minimize() / host.close()      void
host.startDragging()                void                   programmatic drag (rare; CSS handles most)
host.openExternal(url)              void
host.pickFolder(initial?)           string?                native folder picker dialog
host.startupGetEnabled()            bool                   HKCU Run key (Win) / LaunchAgent (Mac)
host.startupSetEnabled(enabled)     void
host.readDataStore()                object                 XOR-decoded JSON; read-only browse
host.update.check()                 UpdateInfo?            see §13
host.update.apply(info)             void                   triggers in-process auto-update
```

No `host.showMessage` — the hub renders message overlays in SolidJS. Native folder pickers stay (`IFileOpenDialog` Win, `NSOpenPanel` Mac) — not replaceable with HTML.

`readDataStore` is read-only by design. Plugins write through the core's `window.DataStore.set` inside LCUX; the hub only browses for the Settings → Data tab.

### 8.6 `pengu.i18n`

```
i18n.getSystemLocale()              string                 e.g. "en-US"
```

That's it — `packages/hub/translations.json` stays embedded in the hub bundle (TS-side). Host only tells the UI what locale to default to.

### 8.7 `pengu.fs`, `pengu.path` — minimal escape hatches

```
fs.readText(path)                   string
fs.writeText(path, content)         void
fs.exists(path)                     bool
fs.readDir(path)                    DirEntry[]             [{ name, isDir }]
path.join(...parts)                 string
```

These exist for niche needs that aren't covered by domain methods. Config, plugins, league, datastore — none of these use `fs` anymore; that's the whole point of the redesign. If a new use case needs more, add a domain method, not more `fs.*`.

### 8.8 Push events

```
activation:stateChanged             { active: boolean }    daemon LCUX detect, IFEO toggle, tray toggle
update:available                    { version, url, body } self-updater finds new release
```

Subscribed via `window.addEventListener('activation:stateChanged', e => ...)`. Replaces today's Tauri `event.listen('active-status', ...)`.

---

## 9. Activation

### 9.1 Modes

| Mode | Windows | macOS |
| --- | --- | --- |
| Universal | IFEO write to `HKLM\...\Image File Execution Options\LeagueClientUx.exe\Debugger` | Daemon polls `proc_listpids` for new `LeagueClientUx`, SIGSTOPs it, captures argv+envp via `sysctl(KERN_PROCARGS2)`, `posix_spawn`s a replacement with `DYLD_INSERT_LIBRARIES=core.dylib`. Original LCUX is **left SIGSTOP'd** (not killed) so LeagueClient's `SIGCHLD`-based child-watch keeps Foundation server alive. Helpers inherit the env var via dyld natively. See [`macos-port.md`](./macos-port.md). |
| OnDemand | (not implemented — see below) | Fallback. Daemon `insert_dylib` patches `libEGL.dylib` to load `core.dylib`; restores backup on session delete. Triggers Riot's per-launch repair prompt — known UX cost. |

**Targeted (symlink) is dropped.** Universal already covers the "set and forget" case better (registry, no Developer Mode dance, no `version.dll` proxy). The `Targeted = 1` enum value is reserved in the wire format but never selectable.

**OnDemand on Windows is dropped (2026-05-08).** Originally planned as a 1:1 mirror of macOS (daemon copies `core.dll` → `<LoL>\dwrite.dll` on session create), but the costs outweighed the benefits:
- IFEO is strictly more reliable on Windows (kernel-side image-load redirect, no daemon required, survives reboots).
- The "macOS code parity for cross-platform testing" argument doesn't hold — the action implementations are entirely platform-specific (`InsertDylibAction` on Mac vs `CopyDllAction` on Windows); only the WAMP daemon shell is shared, and that's small.
- "Copy our DLL into the LoL folder" is an AV-relevant pattern; IFEO via `reg.exe` shells through a system-trusted binary and is safer.
- Keeps the Windows tray + HKCU Run startup off the critical path (no daemon to keep alive on login).

The `OnDemand = 2` enum value stays defined (used by macOS); `pengu.activation.listModes()` reports `available: false` for OnDemand on Windows automatically since the registry has no action registered for it.

### 9.2 `IActivationAction`

```csharp
namespace Pengu.Activation;

public interface IActivationAction
{
    Task OnSessionCreatedAsync(LcuxSession session, CancellationToken ct);
    Task OnSessionDeletedAsync(LcuxSession session, CancellationToken ct);
    Task<bool> IsActiveAsync(CancellationToken ct);
    Task<ActivationResult> SetActiveAsync(bool active, CancellationToken ct);
}
```

`Pengu.Activation.RcsDaemon` (in core) subscribes to RCS WAMP and dispatches to whichever `IActivationAction` is registered for the current mode. The action is platform-specific:

- `Pengu.Windows.Activation.IfeoAction` — Universal mode (no daemon involvement; `SetActiveAsync` writes/deletes the IFEO key).
- `Pengu.Windows.Activation.CopyDllAction` — OnDemand on Windows (dropped, see §9.4).
- `Pengu.MacOS.Activation.RespawnAction` — Universal mode on macOS (default). Drives `LcuxWatcher` instead of consuming the WAMP daemon.
- `Pengu.MacOS.Activation.InsertDylibAction` — OnDemand on macOS (fallback).

### 9.3 IFEO write

Today's Tauri impl uses `winreg::RegKey` directly. **The .NET 10 host shells out via `cmd /c reg add ...`** to avoid AV static analysis flagging the binary's import table for `RegSetValueExW`-on-IFEO patterns. v1.1.6's WPF loader does the same:

```
Process.Start(new ProcessStartInfo {
    FileName = "cmd.exe",
    Arguments = "/c reg add \"HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\LeagueClientUx.exe\" /v Debugger /t REG_SZ /d \"<value>\" /f",
    Verb = "runas",                  // UAC prompt
    UseShellExecute = true,
    CreateNoWindow = true,
});
```

Reads use `Microsoft.Win32.RegistryKey.OpenSubKey(KEY_READ)` — read APIs don't trigger AV heuristics.

### 9.4 OnDemand on Windows — dropped

Originally specified here as a CopyDllAction + RcsDaemon pair, mirroring macOS. Decision reversed (2026-05-08): Windows ships only Universal mode. See §9.1 above for the reasoning. The `OnDemand = 2` enum value remains defined and `pengu.activation.listModes()` reports `available: false` for it on Windows automatically (registry has no action registered → registry-driven availability check returns false). If a future build wants to revisit Windows OnDemand, the `IActivationAction` interface and `ActivationActionRegistry` are both already in place — register a `CopyDllAction` and the bridge surface picks it up without further changes.

### 9.5 Universal on macOS — kill-and-respawn with `DYLD_INSERT_LIBRARIES`

This is the macOS default. Full design and rationale in [`macos-port.md`](./macos-port.md); summary here for activation-context completeness.

`Pengu.MacOS.Activation.RespawnAction` runs a `LcuxWatcher` polling `proc_listpids` every 5 ms. On a new pid matching `LeagueClientUx`:

1. `kill(pid, SIGSTOP)` — pre-`cef_initialize`.
2. Re-check `proc_pidpath` (fork-window race guard); if it's now a Helper or another binary, `SIGCONT` and skip.
3. `sysctl(KERN_PROCARGS2)` → original argv + envp.
4. Parse `--install-directory=` from argv → working directory (LCUX uses relative `Plugins/` paths and segfaults if cwd is wrong).
5. `Process.Start` with same argv + envp + added `DYLD_INSERT_LIBRARIES=/abs/path/core.dylib` and the captured working directory.
6. **Do not SIGKILL the original.** A stopped pid passes `kill(pid, 0)` liveness so LeagueClient never sees `SIGCHLD` and keeps its Foundation server up; killing breaks the LeagueClient↔LCUX coordination and our re-spawn gets `ERR_CONNECTION_REFUSED`.

Helpers don't need explicit handling — they're spawned by the new LCUX and inherit `DYLD_INSERT_LIBRARIES` via dyld natively (every Helper bundle is `flags=0x0`). This means `core.dylib` does **not** need a `posix_spawn` interpose on macOS, unlike the Windows side that has a `CreateProcessW` hook for renderer children.

No sudo, no SIP relaxation, no Apple-grant entitlements (e.g. `cs.debugger`), no Riot file modified.

**Side effect:** one stopped LCUX pid persists per launch session. The `ZombieReaper` in [`app-mac-impl.md`](./app-mac-impl.md) §K cleans these up on Pengu shutdown.

The `RcsDaemon` from §9.7 is **not used** in Universal mode — `LcuxWatcher` is its replacement. `RcsDaemon` is still wired for OnDemand fallback (§9.6).

### 9.6 OnDemand on macOS — `insert_dylib` ported to C# (fallback)

Retained as a fallback for users who hit a `RespawnAction` failure (e.g. if Riot ever enables hardened runtime on `LeagueClientUx`, breaking the env-var inheritance). The Rust port at [`packages/hub/src-tauri/src/macos/dylib.rs`](../packages/hub/src-tauri/src/macos/dylib.rs) (and the older C binary at `bin/insert_dylib`) **are dropped**; we port the Mach-O parser to C# in `Pengu.MacOS.Activation.InsertDylib`. ~250 LOC C#:

- Recognize FAT binaries (`FAT_MAGIC` / `FAT_CIGAM`); iterate each `fat_arch`.
- For each Mach-O slice (`MH_MAGIC[_64]` / `MH_CIGAM[_64]`):
  - Parse load commands; if `LC_CODE_SIGNATURE` is the last command, strip it (the patched binary won't satisfy the original signature anyway).
  - Append a new `LC_LOAD_DYLIB` load command pointing at `core.dylib`, padded to 8 bytes.
  - Update `mach_header.ncmds` and `sizeofcmds`.
- For fat binaries, fix up `fat_arch.size` and re-pack.
- Use `BinaryReader` / `BinaryWriter` over a `FileStream` opened `r+`. Endianness handled with `BinaryPrimitives.ReverseEndianness` per slice's magic.

Action sequence for OnDemand on macOS:

1. Backup `<LoL>.app/Contents/Frameworks/Chromium Embedded Framework.framework/Libraries/libEGL.dylib` to `libEGL.dylib.bak`.
2. Run `InsertDylib.Patch(libEGL_path, core_dylib_path)`.
3. Emit `activation:stateChanged { active: true }`.
4. On WAMP `Delete`: restore the backup.

### 9.7 Activation result encoding

The Tauri `(stage << 8) | error_kind` exit-code packing scheme is dropped. `ActivationResult` is just:

```csharp
public record ActivationResult(bool Ok, string? Error = null, string? Stage = null);
```

For elevated subprocess flows (the IFEO write needs UAC), we communicate via stdout JSON: the elevated child writes a single `ActivationResult` JSON line to stdout; the parent reads it. AOT-clean (`JsonSerializerContext`), no bit-packing, no opaque exit codes.

### 9.8 RCS WAMP daemon

Used by **OnDemand mode only** on macOS. Universal mode uses `LcuxWatcher` (proc_listpids polling) instead. Shared C# in `Pengu.Activation.RcsDaemon`. Subscribes to `wss://riot:<token>@<addr>/` with `OnJsonApiEvent_product-session_v1_sessions`. Credentials come from the running RiotClient process command line (`--app-port=<n> --remoting-auth-token=<s>`):

- **Windows**: WMI `SELECT CommandLine FROM Win32_Process WHERE Name = 'RiotClientServices.exe'`. Slow but reliable; cache the result for the process lifetime.
- **macOS**: `sysctl KERN_PROC_PID` + `proc_pidinfo PROC_PIDARG_PROCNAMETOPID` + parse args from `/proc/<pid>/cmdline` (fallback: shell out to `ps -ax`).

The WebSocket connection uses `System.Net.WebSockets.ClientWebSocket` with a custom `RemoteCertificateValidationCallback` that accepts RCS's self-signed cert (the cert's CN matches the RCS deployment).

---

## 10. Tray

**Windows: tray dropped (2026-05-08)** alongside the OnDemand decision (§9.1). Universal mode doesn't need a long-running process — the hub opens, the user toggles, the hub closes. There's no daemon to keep alive, no OnDemand-armed state to surface, so the tray's primary job (keep the process resident with a way to bring it back) is moot. We could ship a "minimize to tray" convenience but the Win32 surface (~400 LOC of `Shell_NotifyIcon` + popup menu + dark-mode glue) isn't worth it for that alone.

**macOS: tray required.** Both Universal (LcuxWatcher polling) and OnDemand (RcsDaemon WAMP listener) require a long-running daemon to fire activation when LCUX launches. The tray is how users keep that process alive after closing the hub window.

| Platform | Tray | Close-button behavior |
| --- | --- | --- |
| Windows | not shipped | exits process |
| macOS | `NSStatusItem` + `NSMenu` | hides window via `[NSWindow orderOut:]`; daemon keeps running |

**macOS tray menu items:**

```
Pengu Loader v1.2.0          (disabled, version header)
─────────────────────────────
Open hub                     (default — Enter activates)
─────────────────────────────
Activated                    (checkmark = current state; click toggles via InsertDylibAction)
─────────────────────────────
Open plugins folder
Reveal core.dylib
─────────────────────────────
Quit
```

Tooltip reflects state: `"Pengu — Watching for League"`, `"Pengu — Off"`.

**Implementation (macOS only)** — lands in milestone E. `NSStatusItem` + `NSMenu` via the AppKit bindings shipped with the .NET 10 macOS workload. No balloon notifications.

---

## 11. Data layout

### 11.1 User data root

| Platform | Path |
| --- | --- |
| Windows | `%PROGRAMDATA%\.pengu\` (= `C:\ProgramData\.pengu\`) |
| macOS | `~/Library/Application Support/Pengu/` |

Holds:
- `config` — the ini-style config file
- `datastore` — XOR-encoded JSON, owned by the core's `window.DataStore`
- `plugins/` — user plugins

**Why `%PROGRAMDATA%` (machine-wide) on Windows.** Universal mode writes to `HKLM\...\IFEO\LeagueClientUx.exe\Debugger`, which is system-wide — every user on the machine gets the IFEO redirect when they launch LoL. If config + plugins lived under `%LOCALAPPDATA%` (per-user), User B logging in would see IFEO firing into core.dll but no plugins (their AppData has nothing). Sharing the data root with the activation scope keeps the model consistent: one machine, one set of plugins, one IFEO state.

**ACL setup**. `C:\ProgramData\.pengu\` is created on first launch and given `Authenticated Users: Modify` with `ContainerInherit | ObjectInherit` flags so all current and future descendants are read+write for any local user. The first-launch creator owns the dir and can set ACLs without admin; subsequent launches verify and no-op (idempotent). Helper: `Pengu.Windows.Native.ProgramDataAcl.EnsureWritableByEveryone(path)`.

**Trust note**. Anyone on the machine can edit Pengu's config or drop a plugin once writes are open. Plugins run with full DOM access inside LCUX, so a malicious local user could backdoor another user's League session. v1.1.6's `Program Files\Pengu Loader\plugins\` after admin install had a similar shape. Pengu is for trusted single-user / family-PC contexts — two distrusting users probably shouldn't share a Pengu install.

**WebView2 user-data folder is per-user, not under the data root.** Path: `%LOCALAPPDATA%\Pengu\WebView2\`. WebView2 stores cookies / IndexedDB / GPU shader cache for THIS user's session — different concern from activation/plugin state, and the WebView2 docs explicitly say the folder shouldn't be machine-wide. Each user gets their own browser cache.

### 11.2 Logs

`<exe_dir>/logs/<yyyy-MM-dd_HHmmss>_<pid>.log` — **next to the exe, not under `%LOCALAPPDATA%`**. Easier for "send me your log" support requests; users don't have to dig through AppData.

Per-launch file (no rotation). On startup, prune `logs/*.log` older than 7 days.

### 11.3 First-launch migration

On every host startup, before any other work:

1. Check `<exe_dir>/config` exists.
2. Check `<data_root>/config` doesn't exist.
3. If both → move `<exe_dir>/config` and `<exe_dir>/plugins/` to `<data_root>/`.

One-shot migrator. ~30 LOC. Subsequent launches no-op.

### 11.4 C++ core path resolution

The core's `Config::loader_dir()` and friends today resolve relative to the running module's path:
- IFEO mode → loader_dir = `<install>` (where `core.dll` is)
- OnDemand mode → loader_dir = `<LoL>` (where `dwrite.dll` was copied) — wrong.

Update in `core/src/config.cc`:

```cpp
path config::known_data_dir() {
#if OS_WIN
    // %PROGRAMDATA%\.pengu — machine-wide, matches WindowsHost.DataRoot.
    wchar_t buf[2048];
    GetEnvironmentVariableW(L"ProgramData", buf, _countof(buf));
    return std::wstring(buf) + L"\\.pengu";
#elif OS_MAC
    return std::string(getenv("HOME")) + "/Library/Application Support/Pengu";
#endif
}

path config::loader_dir() {
    auto data = known_data_dir();
    if (!data.empty() && std::filesystem::exists(data / "config"))
        return data;
    return module_dir();   // fallback: directory of the running module
}
```

The fallback preserves OG behavior for users who haven't migrated yet (or run with a custom layout). On macOS there's no ProgramData equivalent for "machine-wide, user-writable"; OnDemand is per-user there (each user runs their own hub) and the per-user `~/Library/Application Support/Pengu/` is the correct shape.

### 11.5 No portable mode

We do not ship a `.portable` marker file or `PENGU_DATA_DIR` env var. Drops the v1.1.6 portable usage pattern; users who depend on it should keep using v1.1.6 until ready to migrate.

---

## 12. Auto-startup

OnDemand requires the host process to be running when the user starts LoL, so auto-startup matters there. Windows runs Universal-only and doesn't need launch-on-login.

| Platform | Mechanism | Default |
| --- | --- | --- |
| Windows | `HKCU\Software\Microsoft\Windows\CurrentVersion\Run\Pengu = "<exe>"` | off, hub UI doesn't surface the toggle |
| macOS | `~/Library/LaunchAgents/com.pengu.lol.plist` | toggleable in Settings → Pengu |

The Windows `StartupRegistry` helper and `host.startupGetEnabled` / `host.startupSetEnabled` API stay in place (small surface, useful for the macOS LaunchAgent equivalent and harmless on Windows where the hub UI doesn't expose the toggle). HKCU only on Windows — never HKLM. Per-user, no admin, easy toggle.

---

## 13. Self-update

Port v1.1.6's [`Updater.cs`](https://github.com/PenguLoader/PenguLoader/blob/v1.1.6/loader/Main/Updater.cs) shape — **full auto-update** with download + apply + restart, not just a notification banner.

### 13.1 Flow

1. On hub mount, the SolidJS UI calls `pengu.host.update.check()`.
2. C# fetches `https://api.github.com/repos/PenguLoader/PenguLoader/releases/latest` via `HttpClient` (one-shot, configurable User-Agent). Parses `tag_name`, `body`, and the first `assets[].browser_download_url` matching `*.zip`. Compares `tag_name` to `AppEnv.Version` via `System.Version`.
3. If newer: hub UI shows an "Update available" banner. User clicks → `pengu.host.update.apply(info)`.
4. C# downloads the zip with progress events (emitted as `update:progress { downloaded, total, percent }`), extracts to `<temp>/pengu_update_<rand>/`.
5. While the core is loaded into LCUX, host pesters the user to close LoL (in-app overlay; reuses the message-overlay component).
6. Once safe, host shells out:

   ```
   cmd /c xcopy /s /y "<update_dir>" "<install_dir>" & rd /s /q "<update_dir>" & start "" "<install_dir>\Pengu.exe"
   ```

   With `Verb = "runas"` if `<install_dir>` is under `%PROGRAMFILES%` (write requires admin); otherwise no elevation.
7. Host exits via `Environment.Exit(0)` while the cmd chain runs. The new exe starts a few seconds later.
8. macOS analog: `tar -xf update.tar.gz` + `mv` + `open`.

### 13.2 Failure path

Any step fails → emit `update:failed { reason }` and surface a banner with a "Open release page" button that calls `pengu.host.openExternal(release_url)`. v1.1.6 does the same fallback.

### 13.3 Endpoint hardcoding

`https://api.github.com/repos/PenguLoader/PenguLoader/releases/latest` is hardcoded; no manifest-signing infra (signed via SignPath at the executable level — that's the trust anchor).

---

## 14. Logging

Hand-rolled tiny logger (~50 LOC) in `Pengu.Logging`:

```csharp
Log.Info("RcsDaemon connected pid={Pid}", pid);
Log.Warn("Plugin {Path} JSDoc malformed", path);
Log.Error(ex, "Bridge handler '{Channel}' threw", channel);
```

Plain string formatting via `string.Format` — no Microsoft.Extensions.Logging dependency, no SourceLink, no structured-logging infra. AOT-trivial.

**Sinks:**
- File: `<exe_dir>/logs/<yyyy-MM-dd_HHmmss>_<pid>.log` — per-launch, never rotated within a session.
- Console: only in Debug builds (`<OutputType Condition="'$(Configuration)' == 'Debug'">Exe</OutputType>` / `WinExe` for Release).
- Default level: Information. `--verbose` flag drops to Debug.

**Logged at this granularity:**
- WebView2 lifecycle (env init, controller create, navigation start/complete) — Debug
- WAMP socket connect/disconnect/event — Information
- Activation actions (file copy, IFEO write, insert_dylib) — Information
- Bridge calls — Debug, optionally suppressible
- Errors — always with stack trace

C++ core logs separately (its own `printf` to stderr in debug, silent in release); the host doesn't intercept that.

---

## 15. AOT considerations

`<PublishAot>true</PublishAot>` on both heads. Required disciplines:

- **JSON**: every type crossing the bridge boundary (request args, reply results, push event payloads, config snapshot, plugin info, etc.) gets a `[JsonSerializable(typeof(T))]` entry in `Pengu.PenguJsonContext`. The source generator emits zero-reflection serializers.
- **No reflection-based dispatch**: the bridge dispatcher is fully source-genned by `Pengu.Gen`. Method invocation, arg deserialization, result serialization all in generated code.
- **WebView2 binding**: `Diga.WebView2.Interop.AOT` instead of `Microsoft.Web.WebView2`.
- **Custom dispatcher**: PeekMessage / NSRunLoop instead of WPF/WinForms message pumps.
- **Trimming**: `<TrimMode>full</TrimMode>` is fine since we don't depend on reflection-heavy libraries.

Expected AOT-published binary sizes:
- `Pengu.exe` (Windows): ~12 MB
- `Pengu` (macOS bundle): ~14 MB

---

## 16. Migration considerations and cross-cutting changes

Implementing `app/` requires changes outside `app/` itself:

- **C++ core path resolution** — ~40 LOC update to `core/src/config.cc` (data root lookup with fallback). Has to land before users pick up `app/` so old installs still work in OnDemand.
- **Hub Tauri removal** — drop `@tauri-apps/api` from `packages/hub/package.json`; rewrite ~30–50 call sites in `packages/hub/src/` to use `pengu.*`. Remove the `@std/ini` dependency (`Config` becomes a thin signal cache around `pengu.config.read()` / `pengu.config.write(patch)`).
- **`packages/hub/src-tauri/` deletion** — once `app/` is feature-complete and shipping. Until then the Rust loader is the production path.
- **`bin/insert_dylib` (C binary) deletion** — replaced by `Pengu.MacOS.Activation.InsertDylib`.
- **`packages/hub/src-tauri/src/macos/dylib.rs` deletion** — same.
- **`tauri.conf.json`** — no longer needed; bundle config moves into `app/Pengu.Windows.csproj` / `Pengu.MacOS.csproj` MSBuild.
- **CI** — needs Node + pnpm + .NET 10 SDK + macOS workload; both heads built per-platform. Code signing (SignPath on Win, `codesign` + `notarytool` on Mac) post-publish.

---

## 17. Open questions deferred to implementation

These weren't decided in design and will fall out naturally:

- macOS NSWindow chrome details (exact traffic-light handling with `fullSizeContentView`, vibrancy material).
- Build-time icon assets — carry over from `packages/hub/src-tauri/icons/` to per-head `Assets/`.
- Activation testability strategy on Windows — likely a tiny RCS-WAMP fake the daemon can be pointed at via env var for dev.
- macOS code signing & notarization — handled in CI, not the host's concern.
- `--reset-config`, `--purge-data` CLI flags for support diagnostics — add as needed.

---

## 18. References

- [`docs/design.md`](./design.md) — v1.2.0 overall architecture (core + preload + hub).
- [`packages/hub/src-tauri/`](../packages/hub/src-tauri/) — Rust loader being replaced.
- [v1.1.6 `loader/`](https://github.com/PenguLoader/PenguLoader/tree/v1.1.6/loader) — WPF .NET Framework loader; source for the IFEO-via-cmd pattern, mutex UUID, broadcast-message single-instance pattern, and self-update flow.
- [PenguLoader/plugin-store](https://github.com/PenguLoader/plugin-store) — placeholder YAML registry.
