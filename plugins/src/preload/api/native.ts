// @ts-ignore
export const native: Native = window.__native;

// @ts-ignore
delete window.__native;

interface Native {
  OpenDevTools: (remote: boolean) => void;
  OpenAssetsFolder: () => void;
  OpenPluginsFolder: (path?: string) => boolean;
  ReloadClient: () => void;

  GetWindowEffect: () => string;
  SetWindowEffect: (name: string | false, options?: any) => boolean;
  SetWindowTheme: (theme: string) => void;

  LoadDataStore: () => string;
  SaveDataStore: (data: string) => void;

  CreateAuthCallbackURL: () => string;
  AddAuthCallback: (url: string, cb: Function) => void;
  RemoveAuthCallback: (url: string) => void;
}