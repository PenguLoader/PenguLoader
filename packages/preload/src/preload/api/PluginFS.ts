import { native } from './native';

/**
 * Scoped filesystem for folder plugins, handed to `init(context)` as
 * `context.fs`. Adapted from PR #141 by Ku-Tadao.
 *
 * `PluginFSGrant` is captured once here and then deleted from the native
 * object, so no code that runs after the preload can mint a capability for a
 * folder — only the loader can, and only for a plugin it is about to load.
 * The natives themselves are unreachable too: `native.ts` deletes
 * `window.__native`, leaving this module's binding as the only reference.
 *
 * The returned object is frozen and closes over its token. That makes it a
 * bearer capability, unlike `$write` and `?dir`, which derive their target
 * from the calling script and cannot be handed to anyone. A plugin that passes
 * its `fs` to imported remote code passes that folder along with it — the
 * blast radius is that one plugin's directory, and it is the plugin's choice.
 * See core/src/renderer/v8_pluginfs.cc for why caller identity isn't used.
 */

const grantPluginFS = native.PluginFSGrant;
delete (native as unknown as Record<string, unknown>).PluginFSGrant;

type WriteOptions = {
  append?: boolean
}

type RemoveOptions = {
  recursive?: boolean
}

/** Mirror of the native normalisation so paths look the same on both sides. */
function normalizePath(path: string | undefined) {
  if (!path)
    return '';

  path = String(path).replace(/\\/g, '/');

  while (path.startsWith('./'))
    path = path.substring(2);

  while (path.startsWith('/'))
    path = path.substring(1);

  return path;
}

export function createPluginFS(pluginRoot: string) {
  const token = grantPluginFS(pluginRoot);
  if (!token)
    return undefined;

  return Object.freeze({
    read(path: string) {
      return native.PluginFSRead(token, normalizePath(path));
    },

    write(path: string, content: string, options: WriteOptions = {}) {
      return native.PluginFSWrite(token, normalizePath(path), String(content), Boolean(options.append));
    },

    mkdir(path: string) {
      return native.PluginFSMkdir(token, normalizePath(path));
    },

    stat(path: string = '') {
      return native.PluginFSStat(token, normalizePath(path));
    },

    ls(path: string = '') {
      return native.PluginFSLs(token, normalizePath(path));
    },

    rm(path: string, options: RemoveOptions = {}) {
      return native.PluginFSRemove(token, normalizePath(path), Boolean(options.recursive));
    },
  });
}
