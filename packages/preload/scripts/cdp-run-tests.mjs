// Reload LCUX and capture console output + test results.
// Used to drive bin/plugins/pengu-tests.js.

const CDP_URL = process.env.CDP_URL || 'http://localhost:8080';
const TIMEOUT_MS = Number(process.env.TIMEOUT_MS || 60000);

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

const consoleMessages = [];
ws.addEventListener('message', ev => {
  const msg = JSON.parse(ev.data);
  if (msg.id && pending.has(msg.id)) {
    const { r, j } = pending.get(msg.id); pending.delete(msg.id);
    if (msg.error) j(new Error(`${msg.error.code}: ${msg.error.message}`));
    else r(msg.result);
    return;
  }
  if (msg.method === 'Runtime.consoleAPICalled') {
    const a = msg.params;
    const argsText = (a.args || []).map(arg => {
      if (arg.type === 'string') return arg.value;
      if ('value' in arg) return JSON.stringify(arg.value);
      if (arg.description) return arg.description;
      return arg.type;
    }).join(' ');
    // Drop %c style args
    const cleaned = argsText.replace(/%c/g, '').replace(/\s+(background:[^"\s]+|color:[^"\s]+)/g, '').trim();
    consoleMessages.push({ type: a.type, text: cleaned });
  }
});

await new Promise(res => ws.addEventListener('open', res, { once: true }));
await send('Runtime.enable');
await send('Page.enable');
await send('Page.reload', { ignoreCache: true });

const start = Date.now();
let last = -1, stableCount = 0;
while (Date.now() - start < TIMEOUT_MS) {
  await new Promise(r => setTimeout(r, 1500));
  try {
    const res = await send('Runtime.evaluate', {
      expression: 'window.__penguTests ? JSON.stringify(window.__penguTests) : null',
      returnByValue: true,
    });
    if (res.result.value) {
      const data = JSON.parse(res.result.value);
      console.log('=== Pengu Tests Results ===');
      console.log(`${data.passed}/${data.total} passed`);
      if (data.failed > 0) {
        console.log('\nFAILURES:');
        for (const r of data.results.filter(r => !r.pass)) {
          console.log(`  ✗ ${r.name}${r.detail ? ' — ' + r.detail : ''}`);
        }
      }
      console.log('\n=== console output (filtered) ===');
      for (const m of consoleMessages) {
        if (/Pengu Test/.test(m.text)) console.log(`  [${m.type}] ${m.text}`);
      }
      ws.close();
      process.exit(data.failed > 0 ? 1 : 0);
    }
  } catch {}
}

console.error(`Timeout after ${TIMEOUT_MS}ms — window.__penguTests never set`);
console.log('\n=== console output (filtered) ===');
for (const m of consoleMessages) {
  if (/Pengu Test/.test(m.text)) console.log(`  [${m.type}] ${m.text}`);
}
ws.close();
process.exit(3);
