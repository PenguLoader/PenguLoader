interface Native {
  OpenDevTools: (remote: boolean) => void;
  OpenAssetsFolder: () => void;
  OpenPluginsFolder: (path?: string) => boolean;
  ReloadClient: () => void;

  GetWindowEffect: () => string;
  SetWindowEffect: (name: string | false, options?: any) => boolean;

  LoadDataStore: () => string;
  SaveDataStore: (data: string) => void;

  CreateAuthCallbackURL: () => string;
  AddAuthCallback: (url: string, cb: Function) => void;
  RemoveAuthCallback: (url: string) => void;
}

const native = window['__native'];
delete window['__native'];

export default <Native>native;