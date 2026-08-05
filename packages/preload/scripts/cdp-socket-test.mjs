// Verify Riot socket subscribe/unsubscribe behavior against the live LCUX
// before refactoring packages/preload/src/preload/rcp/socket.ts.

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
  window.__sockTest = { events: 0, errors: [], stage: 'init', listensForBefore: null, listensForAfter: null };
  function trySetup() {
    if (!window.rcp) { setTimeout(trySetup, 5); return; }
    const ok = window.rcp.preInit('rcp-fe-common-libs', (provider) => {
      try {
        const sock = window.__riotSocket = provider.context.socket;
        const URI = 'OnJsonApiEvent';
        window.__sockTest.listensForBefore = sock.listensFor(URI);
        window.__sockTest.listener = function(data) {
          window.__sockTest.events++;
          window.__sockTest.lastEventKeys = data && typeof data === 'object' ? Object.keys(data) : null;
          window.__sockTest.lastEventThis = this === sock ? 'socket' : (this === undefined ? 'undefined' : (this === null ? 'null' : 'other'));
        };
        sock.subscribe(URI, null, window.__sockTest.listener);
        window.__sockTest.listensForAfter = sock.listensFor(URI);
        window.__sockTest.stage = 'subscribed';
      } catch (e) {
        window.__sockTest.errors.push('subscribe: ' + e.message);
      }
    });
    if (!ok) window.__sockTest.errors.push('preInit returned false');
  }
  trySetup();
})();
`;

await send('Page.enable');
await send('Page.addScriptToEvaluateOnNewDocument', { source: bootstrap });
await send('Page.reload', { ignoreCache: true });

// Wait for plugin announces (8s)
await new Promise(r => setTimeout(r, 8000));

const subscribed = await send('Runtime.evaluate', {
  expression: 'JSON.parse(JSON.stringify(window.__sockTest, (k, v) => typeof v === "function" ? "[fn]" : v))',
  returnByValue: true,
});
console.log('--- after subscribe (waited 8s) ---');
console.log(JSON.stringify(subscribed.result.value, null, 2));

// Wait some more to see if events flow naturally
await new Promise(r => setTimeout(r, 6000));

const after = await send('Runtime.evaluate', {
  expression: 'JSON.parse(JSON.stringify(window.__sockTest, (k, v) => typeof v === "function" ? "[fn]" : v))',
  returnByValue: true,
});
console.log('--- after additional 6s wait ---');
console.log(JSON.stringify(after.result.value, null, 2));

// Test unsubscribe
const unsub = await send('Runtime.evaluate', {
  expression: `(() => {
    const before = window.__riotSocket.listensFor('OnJsonApiEvent');
    window.__riotSocket.unsubscribe('OnJsonApiEvent', null, window.__sockTest.listener);
    const after = window.__riotSocket.listensFor('OnJsonApiEvent');
    return { listensForBeforeUnsub: before, listensForAfterUnsub: after };
  })()`,
  returnByValue: true,
});
console.log('--- unsubscribe test ---');
console.log(JSON.stringify(unsub.result.value, null, 2));

ws.close();
