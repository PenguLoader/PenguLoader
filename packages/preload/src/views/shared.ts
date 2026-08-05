/**
 * Views-side of the core/views handoff — see `preload/shared.ts` for why this
 * isn't a plain import.
 *
 * Read once at module load and removed from `window`, so the global is a
 * transport rather than a permanent surface. The core guarantees it exists:
 * `loader.ts` awaits this chunk before any plugin loads, and the core entry
 * installs it synchronously well before that.
 */
const shared = window.__pshared;
delete window.__pshared;

if (!shared) {
  // Only reachable if views were loaded without the core, which nothing does.
  // Fail loudly rather than handing back undefined and breaking later.
  throw new Error('[pengu] views loaded without the preload core (__pshared missing)');
}

export const rcp = shared.rcp;
export const socket = shared.socket;
