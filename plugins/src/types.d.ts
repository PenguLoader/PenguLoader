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

interface CommandBar {
  addAction: (action) => void
  show: () => void
  update: () => void
}

interface DataStore {
  has: (key: string) => boolean
  get: <T>(key: string, fallback?: T) => T | undefined
  set: (key: string, value: any) => boolean
  remove: (key: string) => boolean
}

type ThemeName = 'light' | 'dark';
type EffectName = 'mica' | 'blurbehind' | 'acrylic' | 'unified';

interface Effect {
  get current(): EffectName | null
  apply: (name: EffectName, options?: any) => boolean
  clear: () => void
  setTheme: (theme: ThemeName) => boolean
}

// globals

namespace Pengu {
  const version: string;
  const superPotato: boolean;
  const plugins: string[];
}

declare const DataStore: DataStore;
declare const CommandBar: CommandBar;
declare const Effect: Effect;

declare const openDevTools: (remote?: boolean) => void;
declare const openAssetsFolder: () => void;
declare const openPluginsFolder: (path?: string) => boolean;
declare const reloadClient: () => void;
declare const restartClient: () => void;
declare const getScriptPath: () => string | undefined;
declare const __llver: string;

declare interface Window {

  DataStore: DataStore;
  CommandBar: CommandBar;
  Effect: Effect;

  openDevTools: typeof openDevTools;
  openAssetsFolder: typeof openAssetsFolder;
  openPluginsFolder: typeof openPluginsFolder;
  reloadClient: typeof reloadClient;
  restartClient: typeof restartClient;
  getScriptPath: typeof getScriptPath;
  __llver: string;
}