// Reload LCUX with a preInit capture hook installed before plugins announce,
// then dump the provider shape Pengu's RCP class sees in `before` callbacks.
//
// Why: the registered API (container.impl) does NOT have `provider.context.socket`.
// That field lives on the provider passed to registrationHandler — only visible
// during the announce → registrar callback chain.

const CDP_URL = process.env.CDP_URL || 'http://localhost:8080';

const targets = await fetch(`${CDP_URL}/json`).then(r => r.json());
const page = targets.find(t => t.type === 'page' && t.url.includes('index.html'));
if (!page) { console.error('no LCUX page'); process.exit(2); }

const ws = new WebSocket(page.webSocketDebuggerUrl);
let nextId = 0;
const pending = new Map();
function send(method, params = {}) {
  const id = ++nextId;
  ws.send(JSON.stringify({ id, method, params }));
  return new Promise((r, j) => pending.set(id, { r, j }));
}
ws.addEventListener('message', ev => {
  const msg = JSON.parse(ev.data);
  if (msg.id && pending.has(msg.id)) {
    const { r, j } = pending.get(msg.id); pending.delete(msg.id);
    if (msg.error) j(new Error(`${msg.error.code}: ${msg.error.message}`));
    else r(msg.result);
  }
});
await new Promise(res => ws.addEventListener('open', res, { once: true }));

// Plugins to inspect — known from prior run plus a few likely-late ones.
const NAMES = [
  'rcp-fe-common-libs','rcp-fe-lol-l10n','rcp-fe-lol-uikit','rcp-fe-ember-libs',
  'rcp-fe-lol-shared-components','rcp-fe-lol-navigation','rcp-fe-lol-parties',
  'rcp-fe-lol-social','rcp-fe-lol-settings','rcp-fe-lol-pft','rcp-fe-lol-tft',
  'rcp-fe-lol-static-assets','rcp-fe-lol-tft-troves',
];

const bootstrap = `
(() => {
  const captured = window.__rcpInspect = { providers: {}, deepCommonLibs: null, errors: [] };
  function describe(v, depth = 0, maxDepth = 2) {
    if (v === null) return 'null';
    if (v === undefined) return 'undefined';
    const t = typeof v;
    if (t !== 'object' && t !== 'function') {
      if (t === 'string') return v.length > 200 ? 'string(' + v.length + ')' : 'string: ' + JSON.stringify(v);
      return t;
    }
    if (depth >= maxDepth) return Array.isArray(v) ? '[Array]' : '[Object keys=' + Object.keys(v).length + ']';
    try {
      if (Array.isArray(v)) return v.length > 5 ? '[Array len=' + v.length + ']' : v.map(x => describe(x, depth + 1, maxDepth));
      const keys = Object.keys(v).slice(0, 30);
      const out = {};
      for (const k of keys) {
        try { out[k] = describe(v[k], depth + 1, maxDepth); }
        catch (e) { out[k] = '<getter threw>'; }
      }
      return out;
    } catch (e) { return '<inspect failed: ' + e.message + '>'; }
  }
  function tryRegister() {
    if (!window.rcp) { setTimeout(tryRegister, 5); return; }
    for (const name of ${JSON.stringify(NAMES)}) {
      const ok = window.rcp.preInit(name, (provider) => {
        try {
          captured.providers[name] = describe(provider);
          if (name === 'rcp-fe-common-libs') {
            captured.deepCommonLibs = describe(provider, 0, 5);
            // Inspect socket prototype methods (own keys miss inherited methods)
            const socket = provider?.context?.socket;
            if (socket) {
              const protoMethods = [];
              let proto = Object.getPrototypeOf(socket);
              let layer = 0;
              while (proto && proto !== Object.prototype && layer < 5) {
                for (const k of Object.getOwnPropertyNames(proto)) {
                  if (k === 'constructor') continue;
                  try {
                    const v = socket[k];
                    if (typeof v === 'function') protoMethods.push({ name: k, layer, sig: v.toString().slice(0, 120).replace(/\\s+/g, ' ') });
                  } catch {}
                }
                proto = Object.getPrototypeOf(proto);
                layer++;
              }
              captured.socketMethods = protoMethods;
              captured.socketCtorName = Object.getPrototypeOf(socket)?.constructor?.name ?? null;
            }
          }
        } catch (e) {
          captured.errors.push(name + ': ' + e.message);
        }
      });
      if (!ok) captured.errors.push(name + ': preInit returned false (already past preInit?)');
    }
    captured.installedAt = Date.now();
  }
  tryRegister();
})();
`;

await send('Page.enable');
await send('Page.addScriptToEvaluateOnNewDocument', { source: bootstrap });
await send('Page.reload', { ignoreCache: true });

// Wait for plugins to register. Loose poll for stabilization.
let last = -1;
for (let i = 0; i < 20; i++) {
  await new Promise(r => setTimeout(r, 1000));
  try {
    const res = await send('Runtime.evaluate', {
      expression: 'Object.keys(window.__rcpInspect?.providers||{}).length',
      returnByValue: true,
    });
    const n = res.result.value;
    if (n === last && n > 0) break;
    last = n;
  } catch {}
}

const result = await send('Runtime.evaluate', {
  expression: 'window.__rcpInspect',
  returnByValue: true,
});
console.log(JSON.stringify(result.result.value, null, 2));
ws.close();
