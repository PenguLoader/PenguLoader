/**
 * Public TypeScript types for Pengu Loader plugin authors.
 *
 * Install once and reference globally — every `window.*` Pengu surface
 * (`window.Pengu`, `window.Settings`, `window.Toast`, `window.DataStore`,
 * `window.CommandBar`, `window.Effect`, `window.os`) becomes type-checked.
 *
 * ```ts
 * /// <reference types="@pengujs/types" />
 * ```
 *
 * Or add `"types": ["@pengujs/types"]` to your tsconfig.json `compilerOptions`.
 */

// =============================================================================
// Plugin module shape
// =============================================================================

// =============================================================================
// RCP (Riot Client Plugin) hooks
// =============================================================================

export interface PluginGraph {
  /** Plugin "affinities" — niche, usually empty. */
  affinities: Record<string, any>;
  /** Map of plugin name → list of its direct dependencies. */
  dependencies: Record<string, string[]>;
  /** Virtual-package resolution: e.g. `rcp-be-lol-rso-auth → rcp-be-rga-rso-auth`. */
  implementations: Record<string, string>;
  /** Plugins loaded lazily / on demand. */
  lazy: Record<string, any>;
  /** Total ordered load sequence chosen by the runtime. */
  sequence: string[];
  /** Backward-compat shims. */
  shims: Record<string, any>;
}

export interface RcpApi {
  /**
   * Register a callback that fires BEFORE the named plugin's registrar.
   * Returns `true` if accepted (plugin is unknown or still in `preInit`),
   * `false` if already past that phase.
   */
  preInit(name: string, callback: (provider: any) => any): boolean;

  /**
   * Register a callback that fires AFTER the named plugin's API is ready.
   * Pass `blocking = true` to delay the plugin's transition to `fulfilled`
   * until the callback resolves.
   * Returns `false` if the plugin is already fulfilled.
   */
  postInit(name: string, callback: (api: any) => any, blocking?: boolean): boolean;

  /**
   * Promise-based alternative to postInit. Resolves with the plugin's API
   * when it reaches `fulfilled`. If already fulfilled, resolves immediately.
   */
  whenReady(name: string): Promise<any>;
  whenReady(names: string[]): Promise<any[]>;

  /**
   * Synchronous lookup of an already-fulfilled plugin's API.
   * Returns `undefined` if the plugin hasn't completed initialization.
   */
  get(name: string): any;

  /** Fetch the plugin dependency graph from `/graph.json` (cached). */
  graph(): Promise<PluginGraph>;

  /** Direct dependencies of `name`. Returns `[]` if unknown or no deps. */
  dependencies(name: string): Promise<string[]>;

  /** Plugins that depend on `name`. O(N) scan; cache if calling frequently. */
  dependents(name: string): Promise<string[]>;

  /** All RCP plugin names in load order. */
  names(): Promise<string[]>;
}

// =============================================================================
// Socket (LCDS event subscription)
// =============================================================================

export interface SocketEventData {
  data: any;
  uri: string;
  eventType: 'Create' | 'Update' | 'Delete';
}

export interface SocketApi {
  /**
   * Subscribe to LCDS events. Pass `'all'` to listen to every event.
   * Returns `{ disconnect }` on success, or `false` on invalid arguments.
   */
  observe(
    api: string,
    listener: (message: SocketEventData) => void,
  ): { disconnect: () => void } | false;

  /**
   * Remove a previously-registered listener. Returns `true` if found and
   * removed, `false` if no matching listener was registered for this api.
   */
  disconnect(
    api: string,
    listener: (message: SocketEventData) => void,
  ): boolean;
}

/**
 * Context passed to a plugin's `init` function.
 * - `rcp` / `socket` mirror the globals exposed on `window` by the preload.
 * - `meta.name` is the plugin's folder name. Omitted for top-level `name.js` plugins.
 */
export interface PluginInitContext {
  rcp: RcpApi;
  socket: SocketApi;
  meta?: { name: string };
}

/** Shape of a plugin module. All exports are optional. */
export interface PluginModule {
  /** Runs at preload time, before any RCP plugin announces. Use to set up `rcp.preInit/postInit` hooks. */
  init?: (context: PluginInitContext) => void | Promise<void>;
  /** Registered as a `window` `'load'` listener. Runs after LCUX's HTML is parsed. */
  load?: () => void;
  /** Same as `load` if exported as default. Non-function defaults are ignored at runtime. */
  default?: () => void;
}

// =============================================================================
// CommandBar
// =============================================================================

export interface Action {
  id?: string;
  name: string | (() => string);
  legend?: string | (() => string);
  tags?: string[];
  icon?: string;
  group?: string | (() => string);
  hidden?: boolean;
  perform?: (id?: string) => unknown;
}

export interface CommandBar {
  addAction: (action: Action) => void;
  show: () => void;
  update: () => void;
}

// =============================================================================
// Toast
// =============================================================================

export type ToastType = 'success' | 'error' | 'info' | 'warning' | 'loading' | 'custom';

export type ToastPosition =
  | 'top-left' | 'top-center' | 'top-right'
  | 'bottom-left' | 'bottom-center' | 'bottom-right';

export interface ToastOptions {
  /** ms; 0, negative, or `Infinity` = sticky. Default 5000; `loading` defaults to sticky. */
  duration?: number;
  position?: ToastPosition;
  /** Override the type's default glyph. */
  icon?: string;
  className?: string;
  /** Reusing an id replaces the existing entry — useful for de-duping. */
  id?: string;
  /** Show the × button. Default true. */
  dismissable?: boolean;
}

export interface Toast {
  success: (message: string, opts?: ToastOptions) => string;
  error:   (message: string, opts?: ToastOptions) => string;
  info:    (message: string, opts?: ToastOptions) => string;
  warning: (message: string, opts?: ToastOptions) => string;
  /** Sticky by default — pair with `update`/`dismiss` for progressive flows. */
  loading: (message: string, opts?: ToastOptions) => string;
  /** Render a custom HTML body. Plugin code is already trusted in this context. */
  custom:  (html: string, opts?: ToastOptions) => string;
  promise: <T>(
    promise: Promise<T>,
    msg: { loading: string; success: string; error: string | ((err: unknown) => string) },
    opts?: ToastOptions,
  ) => Promise<T>;
  /** Omit `id` to dismiss every toast. */
  dismiss: (id?: string) => void;
  update:  (id: string, patch: { message?: string; type?: ToastType; icon?: string }) => void;
}

// =============================================================================
// DataStore (durable cross-launch key/value)
// =============================================================================

export interface DataStore {
  has: (key: string) => boolean;
  /** Sync read against the in-memory mirror; safe to call from plugin `init`. */
  get: <T = unknown>(key: string, fallback?: T) => T | undefined;
  /**
   * Mutates the in-memory mirror immediately and schedules a debounced async
   * commit (latest-wins coalescing on the native side). Returns `false` only
   * on invalid args; `true` means "accepted and will persist soon."
   */
  set: (key: string, value: unknown) => boolean;
  /** Same async-commit semantics as `set`. Returns `true` if the key existed. */
  remove: (key: string) => boolean;
  /**
   * Force the pending debounced write immediately and resolve once it is
   * durable on disk. Power-user method — the common path doesn't need it.
   */
  flush: () => Promise<void>;
}

// =============================================================================
// Effect (window vibrancy / theme)
// =============================================================================

export interface ApplyEffectFn {
  (type: 'transparent' | 'blurbehind' | 'acrylic' | 'unified', options?: { color: string }): void;
  (type: 'mica', options?: { material?: 'auto' | 'mica' | 'acrylic' | 'tabbed' }): void;
  (type: 'vibrancy', options: { material: string; alwaysOn?: boolean }): void;
}

export interface Effect {
  apply: ApplyEffectFn;
  clear: () => void;
  setTheme: (theme: 'light' | 'dark') => void;
}

// =============================================================================
// Settings (this PR)
// =============================================================================

/**
 * Field declaration in a Settings schema. The discriminating `type` field
 * determines what widget the drawer renders and what value the field holds.
 */
export type Field =
  | { type: 'boolean'; label: string; default: boolean; description?: string }
  | { type: 'string';  label: string; default: string;  description?: string; placeholder?: string; multiline?: boolean }
  | { type: 'number';  label: string; default: number;  description?: string; min?: number; max?: number; step?: number; slider?: boolean }
  | { type: 'select';  label: string; default: string;  description?: string; options: ReadonlyArray<{ value: string; label: string }> }
  | { type: 'action';  label: string; description?: string; perform: () => void }
  | { type: 'note';    text: string };

/** Schema is a record of field id → Field. Insertion order is preserved in the rendered form. */
export type Schema = Record<string, Field>;

// =============================================================================
// Writable JSON modules
// =============================================================================

/**
 * Helper type for the `import config from './config.json'` + `await config.$write()`
 * flow. Apply at the import site to surface `$write` on the inferred type:
 *
 * ```ts
 * import _config from './config.json';
 * const config = _config as WritableJson<typeof _config>;
 * config.foo = 123;
 * await config.$write();      // compact
 * await config.$write(2);     // 2-space pretty print
 * await config.$write('\t');  // tab-indented pretty print
 * ```
 *
 * `$write(space?)` mirrors `JSON.stringify`'s third argument. `space` can be
 * a number 0–10 (count of spaces) or a string up to 10 chars long (used as
 * the indent verbatim); anything else is ignored and the output is compact.
 *
 * `$write` only exists on JSON modules whose root is an object or array
 * (primitives can't carry properties). Calling `$write` on a non-writable
 * root throws at runtime; the type does not enforce that distinction.
 */
export type WritableJson<T> = T & {
  readonly $write: (space?: number | string) => Promise<void>;
};

/** Maps a Field to its persisted value type. `action` and `note` have no value. */
export type FieldValue<F> =
    F extends { type: 'boolean'; default: infer D } ? D
  : F extends { type: 'string'; default: infer D } ? D
  : F extends { type: 'number'; default: infer D } ? D
  : F extends { type: 'select'; default: infer D } ? D
  : never;

/**
 * Inferred values object for a Schema. For best inference, write your schema
 * with `as const` so TypeScript narrows literal defaults:
 *
 * ```ts
 * const schema = {
 *   enabled:   { type: 'boolean', label: 'Enabled', default: true },
 *   threshold: { type: 'number',  label: 'Threshold', default: 50, min: 0, max: 100 },
 * } as const;
 * ```
 */
export type InferValues<S extends Schema> = {
  [K in keyof S as FieldValue<S[K]> extends never ? never : K]: FieldValue<S[K]>;
};

export interface SettingsRegister<S extends Schema> {
  /** Stable id — used as the drawer entry key. Convention: plugin folder name. */
  id: string;
  /** Display name in the drawer sidebar. */
  name: string;
  /** Optional one-line description shown under the name. */
  description?: string;
  /** Optional icon glyph (single char / emoji) shown next to the name. */
  icon?: string;
  schema: S;
  /**
   * Optional shortcut, e.g. `'Ctrl+,'` or `'Ctrl+Shift+S'`. Must include at least
   * one modifier (Ctrl/Alt/Shift/Meta). Pressing it opens the drawer with this
   * plugin selected. `Ctrl` matches `Cmd` on macOS automatically.
   *
   * On collision, the latest registration wins and a console warning is emitted.
   */
  hotkey?: string;
  /**
   * Backing state the drawer reads and mutates directly. Pair with a writable-
   * JSON import for persistence:
   *
   * ```ts
   * import config from './config.json';
   * Settings.register({
   *   id: 'foo', name: 'Foo', schema,
   *   state: config,
   *   onChange: () => config.$write(),
   * });
   * ```
   *
   * Missing keys are filled from `schema[key].default` at register time.
   * Omit `state` for ephemeral in-memory settings (lost on reload).
   */
  state?: InferValues<S>;
  /**
   * Fired (debounced ~80 ms) after every mutation — drawer toggles and
   * `set()` calls alike. Use this as your persistence hook (e.g.
   * `() => state.$write()`). Async returns are awaited only to surface
   * rejections in the console; the drawer doesn't block on them.
   */
  onChange?: (values: InferValues<S>) => void | Promise<void>;
}

export interface SettingsHandle<V> {
  /** Returns the current values snapshot. Call this whenever you need the latest state. */
  values: () => V;
  /** Patch values; persists immediately, fires onChange (debounced). */
  set: (patch: Partial<V>) => void;
  /** Tear down the registration. Hotkey is unbound, drawer entry removed. */
  unregister: () => void;
}

export interface Settings {
  register<S extends Schema>(opts: SettingsRegister<S>): SettingsHandle<InferValues<S>>;
  /** Open the drawer programmatically. Pass a plugin id to focus that pane. */
  open: (pluginId?: string) => void;
  /** Close the drawer. */
  close: () => void;
  /** List currently-registered plugins. */
  list: () => Array<{ id: string; name: string }>;
}

// =============================================================================
// Pengu globals
// =============================================================================

export interface PenguGlobal {
  /** Pengu Loader version (e.g. "v1.2.0"). */
  version: string;
  /** True when the user has the super-potato switch on. */
  superPotato: boolean;
  /** True when the auto-update-check toggle is on. Mirrors the hub's setting. */
  autoUpdateCheck: boolean;
  /** Plugin entry paths discovered on disk (relative to the plugins folder). */
  plugins: string[];
  /** True on macOS, false on Windows. */
  isMac: boolean;
}

export interface OsGlobal {
  name: 'win' | 'mac';
  version: string;
  build: string;
}

// =============================================================================
// Window augmentation
// =============================================================================

declare global {
  interface Window {
    rcp: RcpApi;
    Pengu: PenguGlobal;
    os: OsGlobal;

    DataStore: DataStore;
    CommandBar: CommandBar;
    Toast: Toast;
    Effect: Effect;
    Settings: Settings;

    openDevTools: () => void;
    openPluginsFolder: (subdir?: string) => void;
    reloadClient: () => void;
    restartClient: () => void;
    getScriptPath: () => string | undefined;
  }
}

export {};
