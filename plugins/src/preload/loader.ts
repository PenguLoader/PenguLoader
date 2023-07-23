import { rcp } from './rcp';

async function loadPlugin(entry: string) {
  let stage = 'load';
  try {
    // Acquire plugin
    const url = `https://plugins/${entry}`;
    const plugin: Plugin = await import(url);

    // Init immediately
    if (typeof plugin.init === 'function') {
      stage = 'initialize';
      await plugin.init({ rcp });
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
    const msg = `Failed to ${stage} plugin "${entry}"\n.`;
    console.error('%c Pengu ', 'background: #183461; color: #fff', msg, err);
  }
}

// Load all plugins asynchronously
const waitable = Promise.all(
  Pengu.plugins
    .filter(n => !/^@default\//i.test(n))
    .map(loadPlugin)
);

// Listen for the first rcp, it's also the first listener
rcp.preInit('rcp-fe-common-libs', async function () {
  // Wait for plugins load
  await waitable;
});

export { }