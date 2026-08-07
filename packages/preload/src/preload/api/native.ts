// @ts-ignore
export const native: Native = window.__native;

// @ts-ignore
delete window.__native;

interface Native {
  OpenDevTools: () => void;
  OpenPluginsFolder: (path?: string) => boolean;
  ReloadClient: () => void;

  SetWindowTheme: (dark: boolean) => void;
  SetWindowVibrancy: (kind: number | null, state?: number) => void;

  // Async DataStore — see core/src/renderer/v8_datastore.cc and
  // packages/preload/src/preload/api/DataStore.ts.
  //
  // One row per key. Load returns a flat [k0, v0, k1, v1, ...] of strings
  // rather than a JSON document, so assembling JSON never happens natively and
  // one bad row cannot take the store down with it. Set / Remove are
  // fire-and-forget; the native writer coalesces per key and commits in
  // batches.
  LoadDataStore:       () => Promise<string[]>;
  LoadLegacyDataStore: () => Promise<string>;      // pre-SQLite blob, migration only
  SetDataStore:        (key: string, json: string) => void;
  RemoveDataStore:     (key: string) => void;
  FlushDataStore:      () => Promise<void>;

  // Writable-JSON $write back-end. Captured + rebound as `window.__pwj` by
  // api/json.ts so the SCRIPT_IMPORT_JSON shim can call into it.
  //
  // Takes no path: the target is derived from the calling script's URL in
  // v8_json_write.cc, so a JSON module can only ever rewrite itself.
  WriteJson: (content: string) => Promise<void>;

  // `?dir` Directory back-end, rebound as `window.__pdir` by api/dir.ts.
  // Also path-less — each derives its folder from the calling script's URL.
  DirExists: () => Promise<boolean>;
  DirFiles:  () => Promise<string[]>;
  DirReveal: () => Promise<void>;

  // Scoped folder-plugin filesystem — see api/PluginFS.ts. Unlike the two
  // above these are token-addressed: PluginFSGrant mints a capability for one
  // plugin root and is deleted from this object immediately after the loader
  // captures it, so nothing running later can mint another.
  PluginFSGrant:  (pluginRoot: string) => string | undefined;
  PluginFSRead:   (token: string, path: string) => Promise<string | undefined>;
  PluginFSWrite:  (token: string, path: string, content: string, append: boolean) => Promise<boolean>;
  PluginFSMkdir:  (token: string, path: string) => Promise<boolean>;
  PluginFSStat:   (token: string, path: string) => Promise<FileStat | undefined>;
  PluginFSLs:     (token: string, path: string) => Promise<string[] | undefined>;
  PluginFSRemove: (token: string, path: string, recursive: boolean) => Promise<number>;

  // Per-plugin key/value store — see core/src/renderer/v8_storage.cc and
  // docs/plugin-storage.md. Only the backing library's version so far; the
  // token-addressed surface will follow the PluginFS shape above.
  StorageVersion: () => string;
}
