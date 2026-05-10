export type StoreKind = 'plugins' | 'themes'

export interface StoreListing {
  id: string
  kind: StoreKind
  name: string
  description: string
  details?: string
  repo?: string
  releaseUrl?: string
  releaseTag?: string
  releaseName?: string
  image?: string
  author: {
    name: string
    avatar?: string
    github?: string
  }
  tags: string[]
  discordUrl: string
  updatedAt?: string
  assets: StoreAsset[]
  enriched: boolean
}

export interface StoreAsset {
  name: string
  size: number
  downloadUrl: string
  contentType?: string
}

interface StoreRegistry {
  listings?: StoreListing[]
}

export const StoreManager = {
  async fetchListingsProgressive(
    onListing: (listing: StoreListing) => void,
  ): Promise<void> {
    const registry = await fetchStaticRegistry()

    for (const listing of registry.listings ?? []) {
      if (!isStoreListing(listing)) continue
      onListing({ ...listing, enriched: true })
    }
  },

  async fetchListings(): Promise<Record<StoreKind, StoreListing[]>> {
    const result: Record<StoreKind, StoreListing[]> = {
      plugins: [],
      themes: [],
    }

    const registry = await fetchStaticRegistry()
    for (const listing of registry.listings ?? []) {
      if (!isStoreListing(listing)) continue
      result[listing.kind].push({ ...listing, enriched: true })
    }

    return result
  },
}

async function fetchStaticRegistry(): Promise<StoreRegistry> {
  const urls = [
    'https://penguloader.github.io/plugin-store/registry/store.json',
    'https://raw.githubusercontent.com/Ku-Tadao/plugin-hub/feature/discord-forum-registry/registry/store.json',
    'https://raw.githubusercontent.com/PenguLoader/plugin-store/main/registry/store.json',
  ]

  let lastError: unknown = null

  for (const url of urls) {
    try {
      const response = await fetch(url, { cache: 'no-store' })
      if (!response.ok) throw new Error(`HTTP ${response.status}`)
      return await response.json() as StoreRegistry
    } catch (error) {
      lastError = error
    }
  }

  throw new Error(`Failed to fetch store registry: ${lastError instanceof Error ? lastError.message : String(lastError)}`)
}

function isStoreListing(value: StoreListing | undefined): value is StoreListing {
  return Boolean(value)
    && typeof value!.id === 'string'
    && (value!.kind === 'plugins' || value!.kind === 'themes')
    && typeof value!.name === 'string'
    && typeof value!.description === 'string'
    && typeof value!.discordUrl === 'string'
    && typeof value!.author?.name === 'string'
    && Array.isArray(value!.tags)
    && Array.isArray(value!.assets)
}
