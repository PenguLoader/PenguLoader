interface Native {
  OpenDevTools: (remote: boolean) => void;
  OpenAssetsFolder: () => void;
  OpenPluginsFolder: () => void;
  ReloadClient: () => void;

  GetWindowEffect: () => string;
  SetWindowEffect: (name: string | false, options?: any) => boolean;

  LoadDataStore: () => string;
  SaveDataStore: (data: string) => void;

  ReadFile: (path:string) => string | undefined;
  WriteFile: (path:string, content: string, enableAppendMode:boolean) => boolean;
  MkDir: (pluginName:string, relativePath:string) => boolean;
  Stat: (path:string) => FileStat | undefined;
  ReadDir: (path:string) => string[] | undefined;
  Remove: (path:string, recursively:boolean) => number;

  CreateAuthCallbackURL: () => string;
  AddAuthCallback: (url: string, cb: Function) => void;
  RemoveAuthCallback: (url: string) => void;
}

const native = window['__native'];
delete window['__native'];

export default <Native>native;