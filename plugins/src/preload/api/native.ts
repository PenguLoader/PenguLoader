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

  LoadDataStore: () => Promise<string>;
  SaveDataStore: (data: string) => Promise<void>;

  DirExists: (path: string) => Promise<boolean>;
  DirFiles: (path: string) => Promise<string[]>;
  DirReveal: (path: string, create: boolean) => void;
}