// Exercise the `https://plugins/` HTTP range implementation against a running
// LCUX, asserting on the real wire headers.
//
// Response headers are read from the CDP Network domain rather than from
// `fetch`, because `Content-Range` and `Accept-Ranges` are not CORS-safelisted
// and a page-side `Headers.get` returns null for both.
//
// Expects `<plugins>/rangetest/data.bin` to be exactly TOTAL bytes where
// byte i == i % 256, so every range can be content-verified and not just
// length-checked.
//
// Usage:
//   node scripts/cdp-range-test.mjs
// Env:
//   CDP_URL   - CDP HTTP base, default http://localhost:8888
//   CDP_PICK  - substring to pick a target, default 'index.html'
//   ASSET     - asset URL, default https://plugins/rangetest/data.bin

const CDP_URL = process.env.CDP_URL || 'http://localhost:8888';
const PICK = process.env.CDP_PICK || 'index.html';
const ASSET = process.env.ASSET || 'https://plugins/rangetest/data.bin';
const TOTAL = 1000;

const at = (i) => i % 256;

// `want` omitted for headers we don't pin (e.g. Content-Range on a 200).
const CASES = [
  { range: 'bytes=0-99',         status: 206, cr: `bytes 0-99/${TOTAL}`,   bytes: 100 },
  { range: 'bytes=0-0',          status: 206, cr: `bytes 0-0/${TOTAL}`,    bytes: 1 },
  { range: 'bytes=500-',         status: 206, cr: `bytes 500-999/${TOTAL}`, bytes: 500 },
  { range: 'bytes=999-999',      status: 206, cr: `bytes 999-999/${TOTAL}`, bytes: 1 },
  { range: 'bytes=-500',         status: 206, cr: `bytes 500-999/${TOTAL}`, bytes: 500 },
  { range: 'bytes=-2000',        status: 206, cr: `bytes 0-999/${TOTAL}`,  bytes: TOTAL },
  // CEF parses the Range itself and synthesises Content-Length from the
  // *unclamped* requested end (99999-500+1), overriding the handler's own
  // header. Body and Content-Range are correct; only that header is off, and
  // it behaved identically before the rewrite. Hence skipCl.
  { range: 'bytes=500-99999',    status: 206, cr: `bytes 500-999/${TOTAL}`, bytes: 500, skipCl: true },
  { range: 'BYTES=0-9',          status: 206, cr: `bytes 0-9/${TOTAL}`,    bytes: 10 },
  { range: 'bytes= 0 - 99 ',     status: 206, cr: `bytes 0-99/${TOTAL}`,   bytes: 100 },

  { range: 'bytes=1000-',        status: 416, cr: `bytes */${TOTAL}`,      bytes: 0 },
  // First-byte-pos beyond EOF: CEF pre-skips, our seek clamps at EOF, and we
  // honestly report the short delta — so CEF calls skip() again, hits eof and
  // fails the request. Semantically right (the range IS unsatisfiable), but it
  // surfaces as a net error instead of our 416. Reaching the 416 would mean
  // over-reporting the skip, which is the exact lie removed from _skip.
  { range: 'bytes=5000-6000',    clientRejected: true },

  // Multi-range is parseable by Chromium but we decline it, so it reaches the
  // handler and must come back as a full 200.
  { range: 'bytes=0-99,200-299', status: 200, bytes: TOTAL },
  { range: null,                 status: 200, bytes: TOTAL },  // no Range at all

  // Chromium's own Range parser rejects these before the scheme handler is
  // ever invoked, failing the fetch with ERR_REQUEST_RANGE_NOT_SATISFIABLE.
  // They are kept as documentation of that boundary: the handler's own guards
  // for them are unreachable from `fetch`, and exist for other request paths.
  { range: 'ab',                 clientRejected: true },
  { range: 'x',                  clientRejected: true },
  { range: 'bytes=',             clientRejected: true },
  { range: 'items=0-99',         clientRejected: true },
  { range: 'bytes=99-0',         clientRejected: true },
  { range: 'bytes=abc-1',        clientRejected: true },
  { range: 'bytes=99999999999999999999-', clientRejected: true },
  { range: 'bytes=-0',           clientRejected: true },
];

const targets = await fetch(`${CDP_URL}/json`).then((r) => r.json());
const page = targets.find(
  (t) => t.type === 'page' && (t.url.includes(PICK) || t.title.includes(PICK)),
);
if (!page) {
  console.error(`no page matching '${PICK}' in CDP targets`);
  process.exit(2);
}

const ws = new WebSocket(page.webSocketDebuggerUrl);
let nextId = 0;
const pending = new Map();
// requestId -> headers, and url -> requestId, so a case can be matched by its
// unique `?c=<n>` marker.
const responses = new Map();
const requests = new Map();
const failed = new Map();
const byUrl = new Map();

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
    return;
  }
  if (msg.method === 'Network.requestWillBeSent') {
    const { requestId, request } = msg.params;
    byUrl.set(request.url, requestId);
    requests.set(requestId, request);
  }
  if (msg.method === 'Network.responseReceived') {
    const { requestId, response } = msg.params;
    responses.set(requestId, response);
    byUrl.set(response.url, requestId);
  }
  if (msg.method === 'Network.loadingFailed') {
    failed.set(msg.params.requestId, msg.params);
  }
});

await new Promise((res, rej) => {
  ws.addEventListener('open', res, { once: true });
  ws.addEventListener('error', rej, { once: true });
});

await send('Network.enable');

// Header lookup is case-insensitive: CEF emits what the handler set, Chromium
// may normalise casing on the way through.
const header = (headers, name) => {
  const key = Object.keys(headers || {}).find(
    (k) => k.toLowerCase() === name.toLowerCase(),
  );
  return key ? headers[key] : null;
};

const results = [];
for (let i = 0; i < CASES.length; i++) {
  const c = CASES[i];
  const url = `${ASSET}?c=${i}`;
  const init = c.range === null ? '{}' : `{headers:{Range:${JSON.stringify(c.range)}}}`;

  const evaluated = await send('Runtime.evaluate', {
    expression: `(async () => {
      const r = await fetch(${JSON.stringify(url)}, ${init});
      const b = new Uint8Array(await r.arrayBuffer());
      return { status: r.status, len: b.length,
               first: b.length ? b[0] : null, last: b.length ? b[b.length - 1] : null };
    })()`,
    returnByValue: true,
    awaitPromise: true,
  });

  if (evaluated.exceptionDetails) {
    for (let n = 0; n < 20 && !byUrl.has(url); n++) {
      await new Promise((r) => setTimeout(r, 25));
    }
    const id = byUrl.get(url);
    const why = failed.get(id);
    results.push({
      c,
      err: why
        ? `${why.errorText}${why.corsErrorStatus ? ` / cors=${why.corsErrorStatus.corsError}` : ''}`
        : evaluated.exceptionDetails.text,
      sentRange: requests.get(id)?.headers
        ? Object.entries(requests.get(id).headers).find(([k]) => k.toLowerCase() === 'range')?.[1] ?? '(not sent)'
        : '(unknown)',
    });
    continue;
  }
  // Give the Network event a moment to land if it hasn't already.
  for (let n = 0; n < 20 && !byUrl.has(url); n++) {
    await new Promise((r) => setTimeout(r, 25));
  }
  const wire = responses.get(byUrl.get(url));
  results.push({ c, got: evaluated.result.value, wire });
}

let failures = 0;
const pad = (s, n) => String(s).padEnd(n);

console.log(
  `${pad('Range', 26)} ${pad('status', 7)} ${pad('bytes', 6)} ${pad('Content-Range', 20)} ${pad('A-R', 6)} result`,
);
console.log('-'.repeat(96));

for (const { c, got, wire, err, sentRange } of results) {
  const label = c.range === null ? '(none)' : c.range;
  if (err) {
    // Expected for headers Chromium refuses to send a range for.
    const expected = c.clientRejected && /RANGE_NOT_SATISFIABLE/.test(err);
    if (!expected) failures++;
    console.log(
      `${pad(label, 26)} ${pad('-', 7)} ${pad('-', 6)} ${pad('-', 20)} ${pad('-', 6)} ` +
        (expected ? 'ok (rejected by Chromium, never reaches handler)' : `NETFAIL ${err} [sent Range: ${sentRange}]`),
    );
    continue;
  }
  if (c.clientRejected) {
    failures++;
    console.log(`${pad(label, 26)} ${pad(got.status, 7)} ${pad(got.len, 6)} ${pad('-', 20)} ${pad('-', 6)} FAIL: expected Chromium to reject this`);
    continue;
  }

  const cr = header(wire?.headers, 'Content-Range');
  const ar = header(wire?.headers, 'Accept-Ranges');
  const cl = header(wire?.headers, 'Content-Length');
  const problems = [];

  if (got.status !== c.status) problems.push(`status ${got.status} != ${c.status}`);
  if (got.len !== c.bytes) problems.push(`body ${got.len} != ${c.bytes}`);
  if (c.cr && cr !== c.cr) problems.push(`Content-Range "${cr}" != "${c.cr}"`);
  // Advertised on every served response, not only 206s.
  if (ar !== 'bytes') problems.push(`Accept-Ranges "${ar}" != "bytes"`);
  // A 206 must declare exactly the bytes it sends.
  if (got.status === 206 && !c.skipCl && cl !== null && Number(cl) !== got.len) {
    problems.push(`Content-Length ${cl} != body ${got.len}`);
  }
  // Content check: the served bytes must be the ones actually requested.
  if (got.len > 0 && c.cr && c.status === 206) {
    const [start, end] = c.cr.match(/bytes (\d+)-(\d+)/).slice(1).map(Number);
    if (got.first !== at(start)) problems.push(`first byte ${got.first} != ${at(start)}`);
    if (got.last !== at(end)) problems.push(`last byte ${got.last} != ${at(end)}`);
  }

  if (problems.length) failures++;
  console.log(
    `${pad(label, 26)} ${pad(got.status, 7)} ${pad(got.len, 6)} ${pad(cr ?? '-', 20)} ${pad(ar ?? '-', 6)} ` +
      (problems.length ? `FAIL: ${problems.join('; ')}` : 'ok'),
  );
}

console.log('-'.repeat(96));
console.log(failures ? `FAILED (${failures}/${results.length})` : `ALL PASS (${results.length} cases)`);
ws.close();
process.exit(failures ? 1 : 0);
