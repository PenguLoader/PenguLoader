import yaml from 'js-yaml'

interface IPlugin {
  name: string
  slug: string
  description: string

  images: string[]
  repo: string
  tags: string[]

  theme: boolean
  auto_update: boolean
}

interface IRegistry {
  name: string
  version: string
  plugins: IPlugin
}

export const StoreManager = new class {

  async fetchPlugins(): Promise<IPlugin> {
    const res = await fetch('https://raw.githack.com/PenguLoader/plugin-store/main/registry/plugins.yml')
    const registry = <IRegistry>await res.text().then(t => yaml.load(t))
    return registry.plugins
  }

}