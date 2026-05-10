import { native } from './native';

const grantPluginFS = native.PluginFSGrant;
delete (native as unknown as Record<string, unknown>).PluginFSGrant;

type WriteOptions = {
  append?: boolean
}

type RemoveOptions = {
  recursive?: boolean
}

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
