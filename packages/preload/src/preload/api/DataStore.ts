import { native } from './native';

/**
 * Persistent key/value store, backed by one row per key in SQLite.
 *
 * Reads are sync against an in-memory Map populated at init. That is why the
 * public `get` / `has` can stay synchronous, and why they must: `Welcome.tsx`
 * calls `get` inline and plugins rely on it during `init`.
 *
 * Writes are what changed. The store used to be a single JSON document that
 * was re-serialized and rewritten in full on every commit, so setting one key
 * cost the price of writing all of them — roughly a second at 100 MB. Now
 * `set` queues a single-row upsert and the native writer coalesces per key.
 *
 * See `core/src/renderer/v8_datastore.cc` and `docs/plugin-storage.md`.
 */

let data_ = new Map<string, unknown>();

/**
 * Read the store. Called from the loader bootstrap before plugins, so plugin
 * `init` can use sync `get` / `has` and see correct values.
 */
export async function initDataStore() {
  try {
    // Flat [k0, v0, k1, v1, ...]. Values are whatever JSON.stringify produced.
    const rows = await native.LoadDataStore();

    for (let i = 0; i + 1 < rows.length; i += 2) {
      try {
        data_.set(rows[i], JSON.parse(rows[i + 1]));
      } catch {
        // One unreadable row is not worth losing the rest of the store over —
        // the whole point of per-key storage is that failures are per-key.
      }
    }

    if (data_.size === 0)
      await migrateLegacy();
  } catch (err) {
    console.warn('Pengu failed to load DataStore, starting empty.', err);
  }
}

/**
 * Import the pre-SQLite store, once.
 *
 * Parsing happens here rather than natively because this is exactly the shape
 * the old implementation already handled, and splitting a JSON document into
 * rows in C++ would mean a JSON parser in the core. The legacy file is left
 * on disk untouched, so a downgrade still finds its data.
 */
async function migrateLegacy() {
  const json = await native.LoadLegacyDataStore();
  if (!json)
    return;

  let object: Record<string, unknown>;
  try {
    object = JSON.parse(json);
  } catch (err) {
    console.warn('Pengu could not parse the legacy DataStore; ignoring it.', err);
    return;
  }

  const entries = Object.entries(object);
  if (entries.length === 0)
    return;

  for (const [key, value] of entries) {
    data_.set(key, value);
    write(key, value);
  }

  console.info('%c Pengu ', 'background: #183461; color: #fff',
    `Migrated ${entries.length} DataStore ${entries.length === 1 ? 'entry' : 'entries'} to SQLite.`);
}

let warnedFull = false;

/** Serialize one value and hand it to the native writer. */
function write(key: string, value: unknown) {
  let json: string;
  try {
    json = JSON.stringify(value);
  } catch (err) {
    console.warn(`Pengu could not serialize DataStore key "${key}".`, err);
    return false;
  }

  // `undefined` has no JSON representation — JSON.stringify returns undefined,
  // not a string. The old whole-document format dropped such keys silently on
  // the next save, so removing it here matches what already happened, just
  // immediately and visibly.
  if (json === undefined) {
    native.RemoveDataStore(key);
    return true;
  }

  if (!native.SetDataStore(key, json)) {
    // Once per session: this fires on every subsequent write, and a plugin
    // hammering a settings slider would otherwise flood the console.
    if (!warnedFull) {
      warnedFull = true;
      console.warn('%c Pengu ', 'background: #183461; color: #fff',
        `DataStore is full — "${key}" and later writes are not being saved. ` +
        `DataStore is shared by every plugin and capped at 128 MB; remove keys ` +
        `you no longer need, or use context.storage, which is per-plugin.`);
    }
    return false;
  }

  return true;
}

window.DataStore = {

  has(key) {
    return data_.has(String(key));
  },

  get(key, fallback) {
    if (typeof key !== 'string') return undefined;
    if (data_.has(key)) return data_.get(key) as any;
    return fallback;
  },

  set(key, value) {
    if (typeof key !== 'string') return false;
    if (!write(key, value)) return false;

    if (value === undefined) data_.delete(key);
    else data_.set(key, value);

    return true;
  },

  remove(key) {
    const k = String(key);
    const result = data_.delete(k);
    if (result) native.RemoveDataStore(k);
    return result;
  },

  async flush() {
    // Writes are queued the moment `set` is called, so there is no debounce to
    // force any more — just wait for the native writer to drain.
    await native.FlushDataStore();
  },

  usage() {
    return native.DataStoreUsage();
  },
};
