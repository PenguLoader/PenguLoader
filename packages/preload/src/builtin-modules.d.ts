/**
 * Ambient declarations for the built-in assets the core serves from inside
 * core.dll, over the reserved `https://plugins/@pengu/` prefix — see
 * `core/src/browser/assets_builtin.h`.
 *
 * Needed because these are imported by *string literal*, which `tsc` resolves
 * eagerly. Plugin imports elsewhere build their URL at runtime, so TypeScript
 * never tries to resolve those and they don't need declaring.
 *
 * Side-effect imports only — `views.js` installs `window.Toast` and
 * `window.Settings` and mounts the UI; it exports nothing.
 *
 * Deliberately a separate file with no top-level import/export. `types.d.ts`
 * is a module, and inside a module `declare module '...'` is read as an
 * augmentation of an existing module rather than an ambient declaration, so
 * putting this there silently fails to apply.
 */
declare module 'https://plugins/@pengu/*';
