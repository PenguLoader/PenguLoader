// Reload LCUX with a hook that records every toast call,
// then dump the trace.

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
  window.__trace = {
    welcomeReads: [],        // each Welcome() call (timestamps + stack)
    createElementPenguRoot: [], // each document.createElement('pengu-root') call
    rootIdAddedTo: [],       // each time we see a pengu-root added to DOM
    customElementCtor: [],   // each PenguRoot constructor invocation (via shadow attach)
  };
  // Instrument DataStore.get('pengu-welcome', ...)
  function tryHookDataStore() {
    if (!window.DataStore?.get) { setTimeout(tryHookDataStore, 5); return; }
    const orig = window.DataStore.get;
    window.DataStore.get = function(key, fallback) {
      if (key === 'pengu-welcome') window.__trace.welcomeReads.push({
        t: Date.now(),
        stack: new Error().stack.split('\\n').slice(2, 8).join(' | ')
      });
      return orig.call(this, key, fallback);
    };
  }
  tryHookDataStore();
  // Wrap document.createElement to log pengu-root creations
  const _origCE = document.createElement.bind(document);
  document.createElement = function(name, opts) {
    const el = _origCE(name, opts);
    if (typeof name === 'string' && name.toLowerCase() === 'pengu-root') {
      window.__trace.createElementPenguRoot.push({
        t: Date.now(),
        stack: new Error().stack.split('\\n').slice(1, 8).join(' | ')
      });
    }
    return el;
  };
  // Wrap attachShadow to log when PenguRoot constructs itself
  const _origAS = Element.prototype.attachShadow;
  Element.prototype.attachShadow = function(...args) {
    if (this.tagName === 'PENGU-ROOT') {
      window.__trace.customElementCtor.push({
        t: Date.now(),
        stack: new Error().stack.split('\\n').slice(1, 8).join(' | ')
      });
    }
    return _origAS.apply(this, args);
  };
  // Watch DOM for pengu-root additions
  function tryObserve() {
    if (!document.body) { setTimeout(tryObserve, 5); return; }
    const obs = new MutationObserver(muts => {
      muts.forEach(m => m.addedNodes.forEach(n => {
        if (n.nodeType === 1 && n.tagName === 'PENGU-ROOT') {
          window.__trace.rootIdAddedTo.push({ t: Date.now(), parent: n.parentElement?.tagName });
        }
      }));
    });
    obs.observe(document.documentElement, { subtree: true, childList: true });
  }
  tryObserve();
})();
`;

await send('Page.enable');
await send('Page.addScriptToEvaluateOnNewDocument', { source: bootstrap });
await send('Page.reload', { ignoreCache: true });

// Wait for Welcome's onMount + 1s buffer
await new Promise(r => setTimeout(r, 12000));

const result = await send('Runtime.evaluate', {
  expression: `(() => {
    const t = window.__trace || {};
    t.penguRootElements = document.querySelectorAll('pengu-root').length;
    return t;
  })()`,
  returnByValue: true,
});
console.log(JSON.stringify(result.result.value, null, 2));
ws.close();
