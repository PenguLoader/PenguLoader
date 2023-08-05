/// <reference types="vite/client" />

interface Plugin {
  init?: (context: any) => any
  load?: () => any
  default?: Function | any
}

interface RcpAnnouceEvent extends CustomEvent {
  errorHandler: () => any
  registrationHandler: (registrar: (e) => Promise<any>) => Promise<any> | void
}

// built-in types

interface AuthCallback {
  createURL: () => string
  readResponse: (url: string, timeout?: number) => Promise<string | null>
}

interface DataStore {
  has: (key: string) => boolean
  get: <T>(key: string, fallback?: T) => T | undefined
  set: (key: string, value: any) => boolean
  remove: (key: string) => boolean
}

type EffectName = 'mica' | 'blurbehind' | 'acrylic' | 'unified';

interface Effect {
  get current(): EffectName | null
  apply: (name: EffectName, options?: any) => boolean
  clear: () => void
}

interface FileStat{
  fileName: string
  length: number
  isDir: boolean
}

interface PluginFS {
  read: (path: string) => Promise<string | undefined>
  write: (path: string, content: string, enableAppendMode: boolean) => Promise<boolean>
  mkdir: (path: string) => Promise<boolean>
  stat: (path: string) => Promise<FileStat | undefined>
  readDir: (path: string) => Promise<string[] | undefined>
  rm: (path: string, recursively:boolean) => Promise<number>
}

// globals

namespace Pengu {
  const version: string;
  const superPotato: boolean;
  const plugins: string[];
}

// declare const AuthCallback: AuthCallback;
declare const DataStore: DataStore;
declare const Effect: Effect;
declare const PluginFS: PluginFS;

declare const openDevTools: (remote?: boolean) => void;
declare const openAssetsFolder: () => void;
declare const openPluginsFolder: () => void;
declare const reloadClient: () => void;
declare const restartClient: () => void;
declare const getScriptPath: () => string | undefined;
declare const __llver: string;

declare interface Window {

  // AuthCallback: AuthCallback;
  DataStore: DataStore;
  Effect: Effect;
  PluginFS:PluginFS;

  openDevTools: typeof openDevTools;
  openAssetsFolder: typeof openAssetsFolder;
  openPluginsFolder: typeof openPluginsFolder;
  reloadClient: typeof reloadClient;
  restartClient: typeof restartClient;
  getScriptPath: typeof getScriptPath;
  __llver: string;
}