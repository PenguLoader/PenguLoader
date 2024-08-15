import { join } from '@tauri-apps/api/path'
import { exists, readTextFile, readDir, createDir } from '@tauri-apps/api/fs'
import { Shell } from './shell'
import { Config } from './config'
import { getHash } from './utils'

export interface PluginInfo {
  name: string
  description?: string
  author?: string
  link?: string

  path: string
  entryPath: string
  hash: number
}

export const PluginManager = new class {

  private disabledSet = new Set<number>()

  private getDir() {
    let path = Config.get('app', 'plugins_dir', '')
    if (!path || path.startsWith('.')) {
      path = Config.basePath('plugins')
    }
    return path
  }

  async openFolder() {
    const dir = this.getDir()
    if (!await exists(dir)) {
      await createDir(dir, {
        recursive: true
      })
    }
    await Shell.expandFolder(dir)
  }

  /**
   * Get all plugins.
   */
  async getPlugins() {
    const dir = this.getDir()
    const plugins = Array<PluginInfo>()

    this.disabledSet = this.fetchDisabledSet()

    function push(name: string, _path: string, entry: string) {
      const dir2 = dir.replace(/\\/g, '/')
      const url = entry.replace(/\\/g, '/')

      const shortPath = url.replace(dir2, '').substring(1)
      const hash = getHash(shortPath.toLowerCase())

      plugins.push({
        name: name,
        path: shortPath,
        entryPath: entry,
        hash: hash,
      })
    }

    if (await exists(dir)) {
      const ref = { entry: '' }
      for (const file of await readDir(dir)) {
        if (file.children) {
          // scan @author folder
          if (file.name!.startsWith('@')) {
            for (let subdir of await readDir(file.path)) {
              // subfolder plugin that contains index.js
              if (subdir.children
                && this.allowedName(subdir.name)
                && await this.hasIndex(subdir.path, ref)) {
                const name = `${file.name}/${subdir.name}`
                push(name, subdir.path, ref.entry)
              }
            }
          }
          // subfolder plugin that contains index.js
          else if (this.allowedName(file.name) && await this.hasIndex(file.path, ref)) {
            push(file.name!, file.path, ref.entry)
          }
        }
        // top-level file plugin
        else if (this.allowedName(file.name) && await this.isIndex(file.path)) {
          const name = file.name!.substring(0, file.name!.lastIndexOf('.'))
          push(name, file.path, file.path)
        }
      }

      // parse metadata
      for (const plugin of plugins) {
        await this.parsePluginEntry(plugin)
      }
    }

    return plugins
  }

  isEnabled(hash: number) {
    return !this.disabledSet.has(hash)
  }

  async toggleState(hash: number) {
    if (this.disabledSet.has(hash)) {
      this.disabledSet.delete(hash)
    } else {
      this.disabledSet.add(hash)
    }

    const value = [...this.disabledSet].map(x => x.toString(16)).join()
    Config.set('app', 'disabled_plugins', value)
    await Config.save()

    return this.isEnabled(hash)
  }

  private allowedName(name?: string) {
    return typeof name === 'string'
      && !name.startsWith('_')
      && !name.startsWith('.')
  }

  private async isIndex(path: string, ref?: { entry: string }) {
    if (path.endsWith('.js') && await exists(path) || await exists(path += '_')) {
      if (typeof ref === 'object') {
        ref.entry = path
      }
      return true
    }
    return false
  }

  private async hasIndex(dir: string, ref: { entry: string }) {
    const path = await join(dir, 'index.js')
    return await this.isIndex(path, ref)
  }

  private async parsePluginEntry(plugin: PluginInfo) {
    if (await exists(plugin.entryPath)) {
      const content = await readTextFile(plugin.entryPath)
      const description = this.getTagValue(content, 'description')
      const author = this.getTagValue(content, 'author')
      const link = this.getTagValue(content, 'link')

      if (description)
        plugin.description = description

      if (author)
        plugin.author = author.includes('#') ? author : '@' + author

      if (link.startsWith('https://'))
        plugin.link = link
    }
  }

  // Parse @tag in jsdoc
  private getTagValue(jsdoc: string, tag: string) {
    const regex = new RegExp(`@${tag}\\s+(.+)`)
    const match = regex.exec(jsdoc)
    if (match) {
      return match[1].trim()
    }
    return ''
  }

  private fetchDisabledSet() {
    const set = new Set<number>()
    const rawSet = <string>Config.get('app', 'disabled_plugins', '')
    const hashes = rawSet.split(',')

    for (const hash of hashes) {
      const num = parseInt(hash.trim(), 16)
      if (num) set.add(num)
    }

    return set
  }
}