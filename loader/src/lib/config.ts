import { Accessor, createRoot, createSignal } from 'solid-js'
import { fs, invoke } from '@tauri-apps/api'
import { ActivationMode } from './core-module'
import { IniMap } from '@std/ini'

const defaultConfig = {
  app: {
    language: 'en',
    plugins_dir: '',
    league_dir: '',
    disabled_plugins: '',
    activation_mode: ActivationMode.Universal,
  },
  client: {
    use_hotkeys: true,
    optimized_client: true,
    silent_mode: false,
    super_potato: false,
    no_logging: false,
    insecure_mode: false,
    use_devtools: false,
    use_riotclient: false,
    use_proxy: false,
  }
}

/// TS is awesome!
type ConfigSection = keyof typeof defaultConfig
type ConfigKey<S extends ConfigSection> = keyof typeof defaultConfig[S]
type ConfigValue<S extends ConfigSection, K extends ConfigKey<S>> = typeof defaultConfig[S][K]

/**
 * Parse string value based on the default value.
 */
function parseValue(val?: string, def?: any) {
  if (val != null) {
    if (typeof def === 'boolean') {
      val = val.trim().toLowerCase()
      return val === '1' || val === 'true'
    } else if (typeof def === 'number') {
      const num = parseInt(val.trim())
      if (!isNaN(num) && !isFinite(num)) {
        return num
      }
    } else {
      return val
    }
  }
  return def
}

export const Config = new class {

  private baseDir = '.'
  private configPath = 'config'
  private ini = new IniMap({ pretty: true })

  /**
   * Get base path to the exec dir.
   * The function does not support relative paths.
   * Don't use Tauri's join() due to async issue.
   */
  basePath(...paths: string[]) {
    return this.baseDir + '/' + paths.join('/')
  }

  /**
   * Load config data from file.
   * Must call it first before doing UI operations.
   * @returns A boolean value indicates the config file already exists.
   */
  async load(): Promise<boolean> {
    const base = await invoke<string>('plugin:config|get_base_dir')
    this.baseDir = base.replace(/\\/g, '/')
    this.configPath = `${this.baseDir}/config`

    if (await fs.exists(this.configPath)) {
      const content = await fs.readTextFile(this.configPath)

      this.ini.parse(content, (key, value, section) => {
        // ensure keys are in default config 
        if (section && section in defaultConfig) {
          const sec = (<any>defaultConfig)[section] as Record<string, any>
          if (key in sec) {
            return parseValue(value, sec[key])
          }
        }
        return value
      })

      return true
    }

    return false
  }

  /**
   * Save config data to file.
   */
  async save() {
    const content = this.ini.toString()
      .replace(/([^\n])\n\[/g, '$1\n\n[')
      .trimStart()
    await fs.writeTextFile(this.configPath, content)
  }

  /**
   * Get intermidiate data.
   */
  get<S extends ConfigSection, K extends ConfigKey<S>>(section: S, key: K, def?: ConfigValue<S, K>): ConfigValue<S, K> {
    return <any>this.ini.get(section, <string>key) ?? def
  }

  /**
   * Set intermidiate data.
   */
  set<S extends ConfigSection, K extends ConfigKey<S>>(section: S, key: K, value: ConfigValue<S, K>) {
    this.ini.set(section, <string>key, value)
  }
}

interface ConfigEntry<T> extends Accessor<T> {
  // (): T
  (value: T): Promise<void>
  (setter: (prev: T) => T): Promise<void>
}

type TransformEntry<T> = {
  [K in keyof T]: T[K] extends object ? TransformEntry<T[K]> : ConfigEntry<T[K]>
}

function defineEntry(section: string, key: string, def: any) {
  const [get, set] = createSignal()
  return function (value?: any) {
    if (arguments.length === 0 || value == null) {
      let val = get()
      if (val === undefined) {
        // @ts-ignore
        val = Config.get(section, key, def)
        set(() => val)
      }
      return val
    } else {
      if (typeof value === 'function') {
        value = value(get())
      }
      // @ts-ignore
      Config.set<T>(section, key, value!)
      set(() => value!)
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

  return config as TransformEntry<typeof defaultConfig>
})

export const useConfig = () => _config