// @ts-ignore
export const native: Native = window.__native;

// @ts-ignore
delete window.__native;

interface Native {
  OpenDevTools: () => void;
  OpenPluginsFolder: (path?: string) => boolean;
  ReloadClient: () => void;

  SetWindowTheme: (dark: boolean) => void;
  SetWindowVibrancy: (kind: number | null, state?: number) => void;

  LoadDataStore: () => string;
  SaveDataStore: (data: string) => void;

  PluginFSGrant: (pluginRoot: string) => string | undefined;
  PluginFSRead: (token: string, path: string) => Promise<string | undefined>;
  PluginFSWrite: (token: string, path: string, content: string, append: boolean) => Promise<boolean>;
  PluginFSMkdir: (token: string, path: string) => Promise<boolean>;
  PluginFSStat: (token: string, path: string) => Promise<FileStat | undefined>;
  PluginFSLs: (token: string, path: string) => Promise<string[] | undefined>;
  PluginFSRemove: (token: string, path: string, recursive: boolean) => Promise<number>;
}