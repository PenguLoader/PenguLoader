import { native } from './native';

class DirectoryModule {
  #path: string;

  constructor(path: string) {
    path = path.substring(16)
    path = decodeURIComponent(path)
    this.#path = path;
  }

  exists() {
    return native.DirExists(this.#path);
  }

  files() {
    return native.DirFiles(this.#path);
  }

  reveal(create?: boolean) {
    native.DirReveal(this.#path, Boolean(create));
  }
}

// @ts-ignore
window.DirectoryModule = DirectoryModule;