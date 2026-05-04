import { native } from './native';

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

export class PluginFileSystem implements PluginFS {
  #token: string;

  constructor(token: string) {
    this.#token = token;
  }

  read(path: string) {
    return native.PluginFSRead(this.#token, normalizePath(path));
  }

  write(path: string, content: string, options: WriteOptions = {}) {
    return native.PluginFSWrite(this.#token, normalizePath(path), String(content), Boolean(options.append));
  }

  mkdir(path: string) {
    return native.PluginFSMkdir(this.#token, normalizePath(path));
  }

  stat(path: string = '') {
    return native.PluginFSStat(this.#token, normalizePath(path));
  }

  ls(path: string = '') {
    return native.PluginFSLs(this.#token, normalizePath(path));
  }

  rm(path: string, options: RemoveOptions = {}) {
    return native.PluginFSRemove(this.#token, normalizePath(path), Boolean(options.recursive));
  }
}

export function createPluginFS(pluginRoot: string) {
  const token = native.PluginFSGrant(pluginRoot);
  if (!token)
    return undefined;

  return Object.freeze(new PluginFileSystem(token));
}