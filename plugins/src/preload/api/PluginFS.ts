import { native } from './native';

const globalFs = {
  read(path: string) {
    return new Promise<string | undefined>((resolve) => {
      const pluginName = getScriptPath()?.match(/\/([^/]+)\/index\.js$/)?.[1]
      const result = native.ReadFile(pluginName + '/' + path);
      resolve(result);
    });
  },
  write(path: string, content: string, enableAppendMode: boolean = false) {
    return new Promise<boolean>((resolve) => {
      const pluginName = getScriptPath()?.match(/\/([^/]+)\/index\.js$/)?.[1]
      const result = native.WriteFile(pluginName + '/' + path, content, enableAppendMode);
      if (!result) {
        console.warn(`PluginFS.write failed on ${path}. Please ask the plugin developer for help.`)
      }
      resolve(result);
    });
  },
  mkdir(path: string) {
    return new Promise<boolean>((resolve) => {
      const pluginName = getScriptPath()?.match(/\/([^/]+)\/index\.js$/)?.[1];
      const result = native.MkDir(pluginName!, path);
      resolve(result);
    });
  },
  stat(path: string) {
    return new Promise<FileStat | undefined>((resolve) => {
      const pluginName = getScriptPath()?.match(/\/([^/]+)\/index\.js$/)?.[1];
      const result = native.Stat(pluginName + '/' + path);
      resolve(result);
    });
  },
  ls(path: string) {
    return new Promise<string[] | undefined>((resolve) => {
      const pluginName = getScriptPath()?.match(/\/([^/]+)\/index\.js$/)?.[1];
      const result = native.Ls(pluginName + '/' + path);
      resolve(result);
    });
  },
  rm(path: string, recursively: boolean = false) {
    return new Promise<number>((resolve) => {
      const pluginName = getScriptPath()?.match(/\/([^/]+)\/index\.js$/)?.[1];
      const result = native.Remove(pluginName + '/' + path, recursively);
      resolve(result);
    });
  }
}
window.Pengu.fs = Object.freeze(globalFs)

function pathCheck(target: any, propertyKey: string, descriptor: PropertyDescriptor) {
  const originalMethod = descriptor.value;
  descriptor.value = function (...args: any[]) {
    const [path, ...rest] = args;
    const doubleDotSlash = /\.\.\//;
    if (doubleDotSlash.test(path)) {
      console.error('%c Pengu ', 'background: #183461; color: #fff', "PluginFS path check failed.");
      return Promise.resolve(undefined);
    }
    return originalMethod.apply(this, [path, ...rest]);
  }
  return descriptor;
}

export class FS {
  constructor(private pluginName: string) {
    this.pluginName = pluginName;
  }

  @pathCheck
  read(path: string) {
    return new Promise<string | undefined>((resolve) => {
      const result = native.ReadFile(this.pluginName + '/' + path);
      resolve(result);
    });
  }

  @pathCheck
  write(path: string, content: string, enableAppendMode: boolean = false) {
    return new Promise<boolean>((resolve) => {
      const result = native.WriteFile(this.pluginName + '/' + path, content, enableAppendMode);
      if (!result) {
        console.warn(`PluginFS.write failed on ${path}.`);
      }
      resolve(result);
    });
  }

  @pathCheck
  mkdir(path: string) {
    return new Promise<boolean>((resolve) => {
      const result = native.MkDir(this.pluginName!, path);
      resolve(result);
    });
  }

  @pathCheck
  stat(path: string) {
    return new Promise<FileStat | undefined>((resolve) => {
      const result = native.Stat(this.pluginName + '/' + path);
      resolve(result);
    });
  }

  @pathCheck
  ls(path: string) {
    return new Promise<string[] | undefined>((resolve) => {
      const result = native.Ls(this.pluginName + '/' + path);
      resolve(result);
    });
  }

  @pathCheck
  rm(path: string, recursively: boolean = false) {
    return new Promise<number>((resolve) => {
      const result = native.Remove(this.pluginName + '/' + path, recursively);
      resolve(result);
    });
  }
}
