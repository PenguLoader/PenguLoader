import { rcp, socket } from './rcp';
import { initDataStore } from './api/DataStore';
import { createPluginFS } from './api/PluginFS';
import type { PluginModule } from '@pengujs/types';

const plugins = window.Pengu.plugins

/**
 * Plugin entries come from `file::read_dir` joins in renderer.cc, so on
 * Windows they arrive backslash-separated (`fstest\index.js`). Everything
 * downstream — the URL, the root detection — wants forward slashes.
 */
function normalizeEntry(entry: string) {
  let normalized = entry.replace(/\\/g, '/');

  while (normalized.startsWith('./'))
    normalized = normalized.substring(2);

  while (normalized.startsWith('/'))
    normalized = normalized.substring(1);

  return normalized;
}

/**
 * The plugin's own folder, or '' when the entry isn't a folder plugin.
 *
 * Recognises `<plugin>/index.js` and `@<author>/<plugin>/index.js`. Anything
 * else — a top-level `name.js`, or a deeper path — yields '', which is what
 * withholds `meta` and `fs`. The old `entry.substring(0, indexOf('/'))` got
 * `@author/plugin/index.js` wrong, handing back just `@author` and scoping a
 * capability to the whole author namespace.
 */
function getDirectoryPluginRoot(entry: string) {
  if (!entry.endsWith('/index.js'))
    return '';

  const pluginRoot = entry.substring(0, entry.length - '/index.js'.length);
  if (!pluginRoot)
    return '';

  const parts = pluginRoot.split('/');
  if (parts.length === 1)
    return pluginRoot;

  if (parts.length === 2 && parts[0].startsWith('@') && parts[1])
    return pluginRoot;

  return '';
}

if ('disabledPlugins' in window.Pengu) {
  const blacklist = new Set<number>
  const disabled = String(window.Pengu.disabledPlugins)
  delete window.Pengu.disabledPlugins

  for (const hash of disabled.split(',')) {
    const num = window.parseInt(hash, 16)
    blacklist.add(num)
  }

  function getHash(str: string) {
    const data = new TextEncoder().encode(str)
    let hash = 0x811c9dc5

    for (const byte of data) {
      hash ^= byte
      hash += (hash << 1) + (hash << 4) + (hash << 7) + (hash << 8) + (hash << 24)
    }

    return hash >>> 0
  }

  function isDisabled(path: string) {
    path = path.toLowerCase().replace(/\\/g, '/')
    return blacklist.has(getHash(path))
  }

  for (let i = plugins.length - 1; i >= 0; --i) {
    const entry = plugins[i]
    if (isDisabled(entry) || /^@default\//i.test(entry)) {
      plugins.splice(i, 1)
    }
  }
}

async function loadPlugin(entry: string) {
  let stage = 'load';
  try {
    // Acquire plugin
    const normalizedEntry = normalizeEntry(entry);
    const url = `https://plugins/${normalizedEntry}`;
    const plugin: PluginModule = await import(url);

    // Init immediately
    if (typeof plugin.init === 'function') {
      stage = 'initialize';
      const pluginRoot = getDirectoryPluginRoot(normalizedEntry);
      const initContext = { rcp, socket };
      // Folder plugins only. A top-level `plugins/name.js` has no directory of
      // its own to scope a filesystem to, so it gets neither meta nor fs —
      // granting it one would have to point at the plugins root itself.
      if (pluginRoot) {
        const meta = { name: pluginRoot };
        initContext['meta'] = meta;

        const fs = createPluginFS(pluginRoot);
        if (fs)
          initContext['fs'] = fs;
      }
      await plugin.init(initContext);
    }

    // Register load
    if (typeof plugin.load === 'function') {
      window.addEventListener('load', plugin.load);
    } else if (typeof plugin.default === 'function') {
      window.addEventListener('load', plugin.default);
    }

    const msg = `Loaded plugin "${entry}".`;
    console.info('%c Pengu ', 'background: #183461; color: #fff', msg);
  } catch (err) {
    const msg = `Failed to ${stage} plugin "${entry}".\n`;
    console.error('%c Pengu ', 'background: #183461; color: #fff', msg, err);
  }
}

// Load all plugins asynchronously. loadPlugin swallows its own errors, so
// Promise.all here only "stalls" if a plugin hangs — top-level await that
// never resolves, infinite loop in init, unawaited fetch to a dead host, etc.
//
// Race that against a hard timeout so a single bad plugin can't deadlock
// rcp-fe-common-libs (and the 35 RCP plugins that depend on it) indefinitely.
// Plugins that finish after the timeout still complete in the background;
// they just don't gate the rest of LCUX from initializing. The trade-off:
// late-finishing plugins miss preInit/postInit hooks for RCP plugins that
// already passed those phases — same gotcha that exists today for any
// subscribe-after-announce.
const PLUGIN_LOAD_TIMEOUT_MS = 15_000;

// Load DataStore from disk before any plugin's `init` — plugin code can then
// rely on sync `DataStore.get` / `has` returning persisted values. Adds a
// one-shot file-read latency (~ms) to the load chain; acceptable trade-off
// for a clean sync API.
const allLoaded = (async () => {
  await initDataStore();
  await Promise.all(plugins.map(loadPlugin));
})();

// Cancel the timer once plugins finish so the warning only fires on a real
// timeout. (Promise.race resolves on the winner but doesn't cancel the loser
// — without clearTimeout the warning would fire 15s after every launch.)
const waitable = new Promise<void>(resolve => {
  const handle = window.setTimeout(() => {
    console.warn('%c Pengu ', 'background: #183461; color: #fff',
      `plugin load exceeded ${PLUGIN_LOAD_TIMEOUT_MS}ms — releasing rcp-fe-common-libs gate. Slow plugins continue loading in the background.`);
    resolve();
  }, PLUGIN_LOAD_TIMEOUT_MS);

  allLoaded.finally(() => {
    window.clearTimeout(handle);
    resolve();
  });
});

// Listen for the first rcp, it's also the first listener
rcp.preInit('rcp-fe-common-libs', async function () {
  // Wait for plugins load (or timeout — see above)
  await waitable;
});

export { }