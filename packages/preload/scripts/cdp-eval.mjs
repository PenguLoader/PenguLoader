// Evaluate JS in the running LCUX renderer over CDP.
// Usage:
//   node scripts/cdp-eval.mjs '<expression>'
//   echo '<expression>' | node scripts/cdp-eval.mjs
// Env:
//   CDP_URL  - CDP HTTP base, default http://localhost:8080
//   CDP_PICK - substring to pick a target by url/title, default 'index.html'

const CDP_URL = process.env.CDP_URL || 'http://localhost:8080';
const PICK = process.env.CDP_PICK || 'index.html';

async function readStdin() {
  if (process.stdin.isTTY) return '';
  let buf = '';
  for await (const chunk of process.stdin) buf += chunk;
  return buf;
}

const expr = process.argv[2] ?? (await readStdin()).trim();
if (!expr) {
  console.error('usage: node cdp-eval.mjs <expression>   (or pipe via stdin)');
  process.exit(1);
}

const targets = await fetch(`${CDP_URL}/json`).then(r => r.json());
const page = targets.find(t => t.type === 'page' && (t.url.includes(PICK) || t.title.includes(PICK)));
if (!page) {
  console.error(`no page matching '${PICK}' in CDP targets:`);
  for (const t of targets) console.error(`  ${t.type} ${t.url}`);
  process.exit(2);
}

const ws = new WebSocket(page.webSocketDebuggerUrl);
let nextId = 0;
const pending = new Map();

function send(method, params = {}) {
  const id = ++nextId;
  ws.send(JSON.stringify({ id, method, params }));
  return new Promise((resolve, reject) => pending.set(id, { resolve, reject }));
}

ws.addEventListener('message', (ev) => {
  const msg = JSON.parse(ev.data);
  if (msg.id && pending.has(msg.id)) {
    const { resolve, reject } = pending.get(msg.id);
    pending.delete(msg.id);
    if (msg.error) reject(new Error(`${msg.error.code}: ${msg.error.message}`));
    else resolve(msg.result);
  }
});

let evaluated = false;
ws.addEventListener('error', (e) => {
  if (evaluated) return; // ignore late errors after the result was returned
  console.error('ws error:', e.message || e);
  process.exit(3);
});

await new Promise((res, rej) => {
  ws.addEventListener('open', res, { once: true });
  ws.addEventListener('error', rej, { once: true });
});

try {
  const result = await send('Runtime.evaluate', {
    expression: `(async () => { return (${expr}); })()`,
    returnByValue: true,
    awaitPromise: true,
    generatePreview: false,
  });

  if (result.exceptionDetails) {
    const ed = result.exceptionDetails;
    console.error('exception:', ed.text);
    if (ed.exception?.description) console.error(ed.exception.description);
    process.exit(4);
  }

  const value = result.result.value;
  if (typeof value === 'string') console.log(value);
  else console.log(JSON.stringify(value, null, 2));
  evaluated = true;
} finally {
  ws.close();
}
