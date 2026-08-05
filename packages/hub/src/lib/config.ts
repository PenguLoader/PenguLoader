import { Accessor, createRoot, createSignal } from 'solid-js'
import { pengu, ActivationMode, type ConfigSnapshot } from './pengu'

const defaultConfig: ConfigSnapshot = {
  app: {
    language: 'en',
    plugins_dir: '',
    disabled_plugins: '',
    activation_mode: ActivationMode.Universal,
    auto_update_check: true,
  },
  client: {
    use_hotkeys: true,
    optimized_client: true,
    silent_mode: false,
    super_potato: false,
    insecure_mode: false,
    use_devtools: false,
    use_riotclient: false,
    use_proxy: false,
    use_transparency: true,
    use_decorations: true,
    use_logging: true,
  },
}

type ConfigSection = keyof ConfigSnapshot
type ConfigKey<S extends ConfigSection> = keyof ConfigSnapshot[S]
type ConfigValue<S extends ConfigSection, K extends ConfigKey<S>> = ConfigSnapshot[S][K]

/**
 * In-memory mirror of the host's `<data_root>/config` file. Reads / writes
 * round-trip through the host bridge (`pengu.config.read/write`); ini
 * parsing + atomic flush live C#-side.
 *
 * Public surface preserved from the v1.1.6/Tauri `Config` singleton: callers
 * do `await Config.load()`, then `Config.get(section, key, def)` and
 * `Config.set(section, key, value)` synchronously, then `await Config.save()`.
 * The `useConfig()` hook returns reactive accessors built on top.
 */
export const Config = new class {

  private baseDir = '.'
  private snapshot: ConfigSnapshot = structuredClone(defaultConfig)
  private loaded = false

  /**
   * Compose a path under the host data root, using the OS-native separator
   * (`\` on Windows, `/` on macOS). The returned string is intended for
   * display and for round-tripping back to the host (which accepts either
   * separator on Win32 dialogs), so we keep it native rather than
   * normalizing to forward-slash.
   */
  basePath(...parts: string[]): string {
    const sep = window.isMac ? '/' : '\\'
    return [this.baseDir, ...parts].join(sep)
  }

  /**
   * Pull the current config from the host. Returns true if the host had a
   * persisted config; false if it returned defaults (first-launch case).
   * Always populates the in-memory snapshot.
   */
  async load(): Promise<boolean> {
    this.baseDir = await pengu.config.getRoot()
    this.snapshot = await pengu.config.read()
    this.loaded = true
    return true
  }

  /** Flush the current snapshot back to the host. */
  async save(): Promise<void> {
    await pengu.config.write(this.snapshot)
  }

  get<S extends ConfigSection, K extends ConfigKey<S>>(
    section: S, key: K, def?: ConfigValue<S, K>
  ): ConfigValue<S, K> {
    const sec = this.snapshot[section] as Record<string, unknown> | undefined
    if (sec && key in sec) {
      const v = sec[key as string] as ConfigValue<S, K>
      if (v !== undefined && v !== null) return v
    }
    return def as ConfigValue<S, K>
  }

  set<S extends ConfigSection, K extends ConfigKey<S>>(
    section: S, key: K, value: ConfigValue<S, K>
  ): void {
    if (!this.loaded) return
    const sec = this.snapshot[section] as Record<string, unknown>
    sec[key as string] = value
  }
}

interface ConfigEntry<T> extends Accessor<T> {
  (value: T): Promise<void>
  (setter: (prev: T) => T): Promise<void>
}

type TransformEntry<T> = {
  [K in keyof T]: T[K] extends object ? TransformEntry<T[K]> : ConfigEntry<T[K]>
}

function defineEntry(section: string, key: string, def: any) {
  const [get, set] = createSignal<any>()
  return function (value?: any) {
    if (arguments.length === 0 || value == null) {
      let val = get()
      if (val === undefined) {
        // @ts-ignore — section/key are stringly-typed at this layer
        val = Config.get(section, key, def)
        set(() => val)
      }
      return val
    } else {
      if (typeof value === 'function') {
        value = value(get())
      }
      // @ts-ignore
      Config.set(section, key, value)
      set(() => value)
      return Config.save()
    }
  }
}

const _config = createRoot(() => {
  const base = defaultConfig as any
  const config: Record<string, object> = {}

  for (const section in defaultConfig) {
    const sec: Record<string, any> = {}
    for (const key in base[section]) {
      const def = base[section][key]
      sec[key] = defineEntry(section, key, def)
    }
    config[section] = sec
  }

  return config as TransformEntry<ConfigSnapshot>
})

export const useConfig = () => _config
