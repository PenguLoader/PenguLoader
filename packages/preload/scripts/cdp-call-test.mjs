// Verify provider.context.socket.call(uri) works against live LCUX.
// Reloads with a hook that stashes Riot's socket on window, then invokes call.

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

const bootstrap = `
(() => {
  const captured = window.__sockTest = { stage: 'init', errors: [] };
  function trySetup() {
    if (!window.rcp) { setTimeout(trySetup, 5); return; }
    const ok = window.rcp.preInit('rcp-fe-common-libs', (provider) => {
      try {
        window.__riotSocket = provider.context.socket;
        captured.stage = 'captured';
      } catch (e) {
        captured.errors.push('capture: ' + e.message);
      }
    });
    if (!ok) captured.errors.push('preInit returned false');
  }
  trySetup();
})();
`;

await send('Page.enable');
await send('Page.addScriptToEvaluateOnNewDocument', { source: bootstrap });
await send('Page.reload', { ignoreCache: true });

// Wait for capture
await new Promise(r => setTimeout(r, 6000));

// Test call on a few common LCDS endpoints
const result = await send('Runtime.evaluate', {
  expression: `(async () => {
    const sock = window.__riotSocket;
    if (!sock) return { error: 'no socket' };
    const out = {};
    const targets = [
      ['/lol-summoner/v1/current-summoner', null, null],
      ['/system/v1/builds', null, null],
      ['/riotclient/region-locale', null, null],
    ];
    for (const [uri, args, kwargs] of targets) {
      try {
        const r = await Promise.race([
          sock.call(uri, args, kwargs),
          new Promise((_, rej) => setTimeout(() => rej(new Error('timeout')), 3000))
        ]);
        out[uri] = {
          type: typeof r,
          isObject: r && typeof r === 'object',
          keys: r && typeof r === 'object' ? Object.keys(r).slice(0, 8) : null,
          truncatedValue: typeof r === 'string' ? r.slice(0, 200) : null,
        };
      } catch (e) {
        out[uri] = { error: e?.message || String(e) };
      }
    }
    return out;
  })()`,
  returnByValue: true,
  awaitPromise: true,
});
console.log(JSON.stringify(result.result.value, null, 2));
ws.close();
