/// <reference types="vite/client" />

namespace globalThis {
  
  var __llver: string;

  function openAssetsFolder(): void;
  function openPluginsFolder(): void;
  function openDevTools(remote?: boolean): void;

  namespace AuthCallback {
    function createURL(): string;
    function readResponse(): Promise<string>;
  }

  namespace DataStore {
    function has(key: string): boolean;
    function get(key: string): any;
    function set(key: string, value: any): Promise<void>;
    function remove(key: string): void;
  }

  namespace Effect {
    type EffectName = 'mica' | 'acrylic' | 'unified' | 'blurbehind';
    const current: EffectName | null;
    function apply(name: EffectName): boolean;
    function apply(name: Exclude<EffectName, 'mica'>, options: { color: string }): boolean;
    function clear(): void;
    function on(event: 'apply', listener: (name: string, options?: object) => any): void;
    function on(event: 'clear', listener: () => any): void;
    function off(event: 'apply', listener: Function): void;
    function off(event: 'clear', listener: Function): void;
  }
}