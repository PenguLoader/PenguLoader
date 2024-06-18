// internal types

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

interface Action {
  id?: string
  name: string | (() => string)
  legend?: string | (() => string)
  tags?: string[]
  icon?: string
  group?: string | (() => string)
  hidden?: boolean
  perform?: (id?: string) => any
}

interface CommandBar {
  addAction: (action) => void
  show: () => void
  update: () => void
}

type ToastPosition = 'top-left' | 'top-center' | 'top-right' | 'bottom-left' | 'bottom-center' | 'bottom-right';

interface ToastOptions {
  position: ToastPosition
  duration: number
}

interface Toast {
  success: (message: string, options?: ToastOptions) => void
  error: (message: string, options?: ToastOptions) => void
  promise: <T>(
    promise: Promise<T>,
    msg: { loading: string, success: string, error: string }
  ) => Promise<T> | void;
}

interface DataStore {
  has: (key: string) => boolean
  get: <T>(key: string, fallback?: T) => T | undefined
  set: (key: string, value: any) => boolean
  remove: (key: string) => boolean
}

type ThemeName = 'light' | 'dark';
type EffectName = 'mica' | 'blurbehind' | 'blur' | 'acrylic' | 'unified' | 'transparent';

interface Effect {
  get current(): EffectName | null
  apply: (name: EffectName, options?: any) => boolean
  clear: () => void
  setTheme: (theme: ThemeName) => boolean
}

interface FileStat {
  fileName: string
  length: number
  isDir: boolean
}

// interface PluginFS {
//   read: (path: string) => Promise<string | undefined>
//   write: (path: string, content: string, enableAppendMode: boolean) => Promise<boolean>
//   mkdir: (path: string) => Promise<boolean>
//   stat: (path: string) => Promise<FileStat | undefined>
//   ls: (path: string) => Promise<string[] | undefined>
//   rm: (path: string, recursively: boolean) => Promise<number>
// }

// globals

declare interface Window {

  DataStore: DataStore;
  CommandBar: CommandBar;
  Toast: Toast;
  Effect: Effect;
  Pengu: {
    version: string
    superPotato: boolean
    silentMode: boolean
    plugins: string[]
    // fs: PluginFS
  };

  openDevTools: typeof openDevTools;
  openAssetsFolder: typeof openAssetsFolder;
  openPluginsFolder: typeof openPluginsFolder;
  reloadClient: typeof reloadClient;
  restartClient: typeof restartClient;
  getScriptPath: typeof getScriptPath;

  __llver: string;
}