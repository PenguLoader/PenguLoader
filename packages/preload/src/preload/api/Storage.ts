import { native } from './native';

/**
 * Per-plugin key/value store, handed to `init(context)` as `context.storage`.
 *
 * `StorageGrant` is captured once here and then deleted from the native
 * object, so no code that runs after the preload can mint a capability for a
 * plugin — only the loader can, and only for one it is about to load. Same
 * shape as `context.fs`, and the same consequence: the returned object is a
 * bearer capability, so passing it to imported third-party code passes that
 * plugin's data along with it.
 *
 * Unlike `DataStore` this is per-plugin, asynchronous, and costs the same to
 * write whether the plugin has stored one record or ten thousand. See
 * docs/plugin-storage.md.
 */

const grantStorage = native.StorageGrant;
delete (native as unknown as Record<string, unknown>).StorageGrant;

function isBuffer(value: unknown): boolean {
  return value instanceof ArrayBuffer || ArrayBuffer.isView(value);
}

export function createPluginStorage(pluginRoot: string) {
  const token = grantStorage(pluginRoot);
  if (!token)
    return undefined;

  return Object.freeze({
    /**
     * The stored value, or `fallback` when the key is absent. Mirrors
     * `DataStore.get(key, fallback)` so a migrating plugin keeps the shape it
     * already knows.
     */
    async get<T = unknown>(key: string, fallback?: T): Promise<T | undefined> {
      const json = await native.StorageGet(token, String(key));
      if (json === undefined)
        return fallback;

      try {
        return JSON.parse(json) as T;
      } catch {
        // A row that cannot be parsed is indistinguishable from one that was
        // never written, as far as the caller can usefully act on it.
        return fallback;
      }
    },

    /**
     * Store any JSON-representable value. `undefined` deletes the key —
     * `JSON.stringify(undefined)` is not a string, so there is nothing to
     * store, and treating it as a delete keeps `get` returning `undefined` for
     * exactly one reason.
     *
     * Throws on a value JSON cannot represent (a circular reference, a
     * BigInt). That is a bug in the caller rather than an I/O failure, and
     * collapsing it into a bare `false` would make it near-undebuggable —
     * `false` is reserved for the storage layer failing.
     */
    set(key: string, value: unknown): Promise<boolean> {
      if (value === undefined)
        return native.StorageDelete(token, String(key));

      if (isBuffer(value)) {
        throw new TypeError(
          'storage.set: binary values are not supported yet — pass a string or a ' +
          'JSON-representable value. See docs/plugin-storage.md §6.3.');
      }

      // Deliberately not caught: see the doc comment above.
      const json = JSON.stringify(value);

      // Reachable without throwing — a function or a symbol stringifies to
      // undefined rather than raising.
      if (json === undefined)
        return native.StorageDelete(token, String(key));

      return native.StorageSet(token, String(key), json);
    },

    has(key: string): Promise<boolean> {
      return native.StorageHas(token, String(key));
    },

    delete(key: string): Promise<boolean> {
      return native.StorageDelete(token, String(key));
    },

    /** Every key, sorted. */
    keys(): Promise<string[]> {
      return native.StorageKeys(token);
    },

    /** Remove every key. Resolves to how many were removed. */
    clear(): Promise<number> {
      return native.StorageClear(token);
    },

    /** Bytes on disk, write-ahead log included. */
    size(): Promise<number> {
      return native.StorageSize(token);
    },
  });
}
