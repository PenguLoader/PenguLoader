import { parse as parseYaml } from 'yaml'
import { pengu, type StorePlugin } from './pengu'

export type { StorePlugin }

/**
 * Plugin store registry browser. The host fetches the YAML body from
 * `https://raw.githack.com/PenguLoader/plugin-store/main/registry/plugins.yml`
 * and forwards it to us as a string; we parse client-side so the AOT host
 * doesn't need a YAML library. Output is browse-only — there is no install
 * automation, per docs/app-hub.md §1.
 */
export const StoreManager = {
  async fetchPlugins(): Promise<StorePlugin[]> {
    const yaml = await pengu.plugins.fetchStoreRegistry()
    const doc = parseYaml(yaml) as { plugins?: unknown } | null
    if (!doc || !Array.isArray(doc.plugins)) return []
    return doc.plugins.filter(isStorePlugin)
  },
}

/**
 * Defensive shape check. The registry is upstream-of-us so a malformed entry
 * shouldn't crash the gallery — drop it and render the rest.
 */
function isStorePlugin(value: unknown): value is StorePlugin {
  if (typeof value !== 'object' || value === null) return false
  const v = value as Record<string, unknown>
  return typeof v.name === 'string'
    && typeof v.slug === 'string'
    && typeof v.description === 'string'
    && typeof v.image === 'string'
    && typeof v.repo === 'string'
    && typeof v.author === 'object' && v.author !== null
    && typeof (v.author as Record<string, unknown>).name === 'string'
    && typeof (v.author as Record<string, unknown>).github === 'string'
    && Array.isArray(v.tags)
}
