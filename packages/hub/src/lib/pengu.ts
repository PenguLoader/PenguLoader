/**
 * Typed facade over the host bridge exposed at `window.pengu`.
 *
 * The C# host (see `app/`) injects a JS shim via `AddScriptToExecuteOnDocumentCreated`
 * that turns property-access + call into `chrome.webview.postMessage({id, channel, args})`
 * and resolves the returned Promise on the matching reply. C#-originated push events
 * arrive as `CustomEvent` on `window` — subscribe with `window.addEventListener('name', e => ...)`.
 *
 * Until the matching C# method lands, calls throw "no bridge handler for X" at runtime.
 * Build-time TS types are forward-looking against `docs/app-hub.md` §8.
 */

// ---------- payload types ----------

export enum ActivationMode {
  Universal = 0,
  /** @deprecated reserved enum value; symlink mode dropped in app/. */
  Targeted = 1,
  OnDemand = 2,
}

export interface ActivationModeInfo {
  mode: ActivationMode
  available: boolean
  requiresAdmin: boolean
}

export interface ActivationResult {
  ok: boolean
  error?: string
  stage?: string
}

/**
 * State of the machine-wide boot (Windows only; null elsewhere).
 *
 * Distinct from `isActive()`: the boot can be installed while this user has
 * Pengu switched off, which is the normal state for a second account on a
 * shared machine.
 */
export interface BootStubState {
  /** The boot exists at its administrator-owned path. */
  installed: boolean
  /** Windows is wired to launch it when the client starts. */
  wired: boolean
  installedVersion?: string
  shippedVersion?: string
  /** This build ships a newer boot than the one installed. */
  updateAvailable: boolean
  /**
   * Why the boot last declined to load a runtime, if it did. The client still
   * launched — it just ran without plugins, which otherwise looks exactly like
   * Pengu quietly not working.
   */
  lastRefusal?: string
}

export interface ConfigSnapshot {
  app: {
    language: string
    plugins_dir: string
    disabled_plugins: string
    activation_mode: ActivationMode
    auto_update_check: boolean
  }
  client: {
    use_hotkeys: boolean
    optimized_client: boolean
    silent_mode: boolean
    super_potato: boolean
    insecure_mode: boolean
    use_devtools: boolean
    use_riotclient: boolean
    use_proxy: boolean
  }
}

export interface PluginInfo {
  name: string
  path: string
  hash: number
  author?: string
  description?: string
  link?: string
  enabled: boolean
}

/**
 * One plugin from the upstream registry at
 * `https://raw.githack.com/PenguLoader/plugin-store/main/registry/plugins.yml`.
 * Parsed client-side from the YAML returned by `pengu.plugins.fetchStoreRegistry()`.
 */
export interface StorePlugin {
  name: string
  slug: string
  description: string
  /** SVG data URL or external image URL — render directly in <img>. */
  image: string
  /** GitHub repo URL. */
  repo: string
  /** Optional Discord channel link. */
  discord?: string
  author: {
    name: string
    /** GitHub handle; combined with `https://github.com/<github>.png` for the avatar. */
    github: string
  }
  auto_update?: boolean
  theme?: boolean
  tags: string[]
}

export interface HostInfo {
  os: 'win' | 'mac'
  version: string
  build: string
  isMac: boolean
  isAdmin: boolean
  locale: string
}

export interface DirEntry { name: string; isDir: boolean }

/**
 * Result of `pengu.update.check()`. Null when the running build is on the
 * latest published release.
 */
export interface UpdateInfo {
  /** Release tag, e.g. "v1.2.0". */
  tag: string
  /** Release body (markdown from the GitHub release). */
  body: string
  /** Web URL to open in the system browser when the user clicks through. */
  url: string
}

// ---------- bridge surface ----------

export interface PenguBridge {
  /** Diagnostic round-trip; always available. */
  ping: {
    echo(message: string): Promise<string>
    add(a: number, b: number): Promise<number>
    version(): Promise<string>
  }

  activation: {
    listModes(): Promise<ActivationModeInfo[]>
    getMode(): Promise<ActivationMode>
    setMode(mode: ActivationMode): Promise<void>
    isActive(): Promise<boolean>
    setActive(active: boolean): Promise<ActivationResult>
    coreExists(): Promise<boolean>
    /** Null on platforms without a machine-wide boot (macOS). */
    getBootState(): Promise<BootStubState | null>
    /** Needs elevation. Deliberately separate from deactivating. */
    removeBoot(): Promise<ActivationResult>
  }

  config: {
    getRoot(): Promise<string>
    getPath(): Promise<string>
    read(): Promise<ConfigSnapshot>
    write(patch: Partial<ConfigSnapshot>): Promise<void>
  }

  plugins: {
    list(): Promise<PluginInfo[]>
    toggleEnabled(path: string): Promise<boolean>
    openFolder(): Promise<void>
    revealInFolder(path: string): Promise<void>
    /** Raw YAML body from the upstream registry; parsed by the hub. */
    fetchStoreRegistry(): Promise<string>
  }

  league: {
    findInstall(): Promise<string | null>
    validateDir(dir: string): Promise<boolean>
  }

  host: {
    getInfo(): Promise<HostInfo>
    minimize(): Promise<void>
    close(): Promise<void>
    startDragging(): Promise<void>
    openExternal(url: string): Promise<void>
    openFolder(path: string): Promise<void>
    revealFile(path: string): Promise<void>
    pickFolder(initial?: string): Promise<string | null>
    startupGetEnabled(): Promise<boolean>
    startupSetEnabled(enabled: boolean): Promise<void>
    readDataStore(): Promise<Record<string, unknown>>
  }

  i18n: {
    getSystemLocale(): Promise<string>
  }

  fs: {
    readText(path: string): Promise<string>
    writeText(path: string, content: string): Promise<void>
    exists(path: string): Promise<boolean>
    readDir(path: string): Promise<DirEntry[]>
  }

  path: {
    /**
     * Join path segments. Bridge wire format requires a single argument, so
     * pass an array (not rest-params). Empty input -> empty string.
     */
    join(parts: string[]): Promise<string>
  }

  update: {
    /**
     * Check the upstream releases endpoint. Resolves to UpdateInfo when a
     * newer release is published, or null when the current build is up-to-
     * date. Rejects on network / HTTP failure.
     */
    check(): Promise<UpdateInfo | null>
  }
}

// ---------- accessor ----------

declare global {
  interface Window {
    pengu: PenguBridge
  }
}

/** Typed accessor for the host bridge. Same object as `window.pengu`. */
export const pengu: PenguBridge = (window as Window & { pengu: PenguBridge }).pengu
