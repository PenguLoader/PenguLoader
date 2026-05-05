import { rcp, socket } from './rcp';
import { createPluginFS } from './api/PluginFS';
import { createPluginBus } from './api/PluginBus';

const plugins = window.Pengu.plugins

function normalizeEntry(entry: string) {
  let normalized = entry.replace(/\\/g, '/');

  while (normalized.startsWith('./'))
    normalized = normalized.substring(2);

  while (normalized.startsWith('/'))
    normalized = normalized.substring(1);

  return normalized;
}

function getDirectoryPluginRoot(entry: string) {
  if (!entry.endsWith('/index.js'))
    return '';

  const pluginRoot = entry.substring(0, entry.length - '/index.js'.length);
  if (!pluginRoot || pluginRoot.includes('/'))
    return '';

  return pluginRoot;
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
    const plugin: Plugin = await import(url);

    // Init immediately
    if (typeof plugin.init === 'function') {
      stage = 'initialize';
      const pluginRoot = getDirectoryPluginRoot(normalizedEntry);
      const pluginName = pluginRoot || normalizedEntry;
      const initContext: PluginContext = { rcp, socket, bus: createPluginBus(pluginName) };

      if (pluginRoot) {
        const meta = { name: pluginRoot };
        const fs = createPluginFS(pluginRoot);
        initContext['meta'] = meta;
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

// Load all plugins asynchronously
const waitable = Promise.all(
  plugins.map(loadPlugin)
);

// Listen for the first rcp, it's also the first listener
rcp.preInit('rcp-fe-common-libs', async function () {
  // Wait for plugins load
  await waitable;
});

export { }