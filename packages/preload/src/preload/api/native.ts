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
  /** false once the 128 MB shared cap is reached — see v8_datastore.cc. */
  SetDataStore:        (key: string, json: string) => boolean;
  RemoveDataStore:     (key: string) => void;
  FlushDataStore:      () => Promise<void>;
  DataStoreUsage:      () => Promise<{ used: number, quota: number }>;

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

  // Per-plugin key/value store — see core/src/renderer/v8_storage.cc,
  // api/Storage.ts and docs/plugin-storage.md.
  //
  // Token-addressed like PluginFS above, and for the same reason: StorageGrant
  // mints a capability for one plugin and is deleted from this object as soon
  // as the loader captures it.
  //
  // Values cross as strings. Serialization is the shim's job, so the native
  // side never parses or builds JSON.
  StorageVersion: () => string;
  StorageGrant:   (pluginRoot: string) => string | undefined;
  StorageGet:     (token: string, key: string) => Promise<string | undefined>;
  /** 0 = failed, 1 = ok, 2 = over quota. See the SET_* codes in v8_storage.cc. */
  StorageSet:     (token: string, key: string, json: string) => Promise<number>;
  StorageHas:     (token: string, key: string) => Promise<boolean>;
  StorageDelete:  (token: string, key: string) => Promise<boolean>;
  StorageKeys:    (token: string) => Promise<string[]>;
  StorageClear:   (token: string) => Promise<number>;
  StorageSize:    (token: string) => Promise<number>;
  StorageUsage:   (token: string) => Promise<{ used: number, quota: number }>;
}
