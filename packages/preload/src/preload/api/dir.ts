import { native } from './native';

/**
 * Wire up the `?dir` Directory module's back-end.
 *
 * The C++ scheme handler emits a SCRIPT_IMPORT_DIR shim (assets_shims.h)
 * whose Directory methods call `window.__pdir`. Same pattern as `__pwj` in
 * `json.ts`, and the same reason it's safe to leave on `window`: the natives
 * take no path. Each one derives its target folder from the calling script's
 * URL off the V8 stack (`v8_dir.cc`), so a script that isn't a `?dir` module
 * gets a rejection rather than a capability.
 *
 * Frozen because the shim reaches through this object on every call — a
 * swapped-out `exists`/`files`/`reveal` would be a way to feed plugin code a
 * fake listing.
 *
 * Must run before any plugin imports a directory. The import chain in
 * `api/index.ts` places this side-effect first, before `loader.ts` starts
 * pulling plugins in.
 */
Object.defineProperty(window, '__pdir', {
  value: Object.freeze({
    exists: native.DirExists,
    files: native.DirFiles,
    reveal: native.DirReveal,
  }),
  writable: false,
  configurable: false,
  enumerable: false,
});

export {};
