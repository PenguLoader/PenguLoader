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

  // ReadFile: (path:string) => string | undefined;
  // WriteFile: (path:string, content: string, enableAppendMode:boolean) => boolean;
  // MkDir: (pluginName:string, relativePath:string) => boolean;
  // Stat: (path:string) => FileStat | undefined;
  // Ls: (path:string) => string[] | undefined;
  // Remove: (path:string, recursively:boolean) => number;
}