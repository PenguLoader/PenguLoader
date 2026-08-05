import { rcp, socket } from './rcp';

/**
 * Hand the RCP registry and socket across the core/views chunk boundary.
 *
 * The two are built as separate bundles, so a plain `import` from views would
 * give it its **own copy** of `./rcp` — and rcp is stateful. Two registries
 * means `preInit` in the core and `whenReady` in views would be talking past
 * each other, which fails silently rather than loudly.
 *
 * Passing the live instances through a global is the same trick `api/native.ts`
 * uses for `window.__native`: views reads it once and deletes it, so nothing
 * lingers on `window` for plugins to find. Views loads before any plugin runs
 * (see loader.ts), so the handoff is always consumed first.
 */
window.__pshared = { rcp, socket };

export { };
