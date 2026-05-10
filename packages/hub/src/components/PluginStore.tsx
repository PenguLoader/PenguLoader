import { Component, createMemo, createSignal, For, Match, onMount, Show, Switch } from 'solid-js'
import { StoreKind, StoreListing, StoreManager } from '~/lib/store'
import { Shell } from '~/lib/shell'
import { LinkIcon, LoaderIcon } from './Icons'

export const PluginStore: Component = () => {
  const [loading, setLoading] = createSignal(true)
  const [error, setError] = createSignal<string | null>(null)
  const [activeTab, setActiveTab] = createSignal<StoreKind>('plugins')
  const [selected, setSelected] = createSignal<StoreListing | null>(null)
  const [listings, setListings] = createSignal<Record<StoreKind, StoreListing[]>>({
    plugins: [],
    themes: [],
  })

  const activeListings = createMemo(() => listings()[activeTab()])
  const selectedListing = createMemo(() => {
    const current = selected()
    if (!current) return null
    return activeListings().find(item => item.id === current.id) ?? null
  })

  onMount(async () => {
    try {
      await StoreManager.fetchListingsProgressive((listing) => {
        setListings(current => ({
          ...current,
          [listing.kind]: upsertListing(current[listing.kind], listing),
        }))
      })
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e))
    } finally {
      setLoading(false)
    }
  })

  return (
    <div class="h-full">
      <Switch>
        <Match when={error()}>
          <div class="h-full flex flex-col items-center justify-center gap-2 text-center px-8">
            <p class="text-destructive">Failed to fetch the community store.</p>
            <p class="text-xs text-muted-foreground">{error()}</p>
          </div>
        </Match>
        <Match when={!error()}>
          <div
            class="h-full overflow-auto p-4"
            onClick={() => setSelected(null)}
          >
            <div class="flex items-center justify-between gap-4">
              <div class="flex rounded-md border border-neutral-700 bg-neutral-950/30 p-1">
                <TabButton
                  active={activeTab() === 'plugins'}
                  count={listings().plugins.length}
                  label="Plugins"
                  onClick={() => {
                    setActiveTab('plugins')
                    setSelected(null)
                  }}
                />
                <TabButton
                  active={activeTab() === 'themes'}
                  count={listings().themes.length}
                  label="Themes"
                  onClick={() => {
                    setActiveTab('themes')
                    setSelected(null)
                  }}
                />
              </div>
              <Show
                when={loading()}
                fallback={<p class="text-xs text-muted-foreground">Discord forum listings enriched with GitHub releases</p>}
              >
                <div class="flex items-center gap-2 text-xs text-muted-foreground">
                  <LoaderIcon class="animate-spin" size={13} />
                  <span>Loading more listings...</span>
                </div>
              </Show>
            </div>

            <div
              class={selectedListing()
                ? 'grid grid-cols-[minmax(0,1fr)_minmax(300px,360px)] gap-4 items-start'
                : 'grid grid-cols-1'}
            >
              <div>
                <div class="grid grid-cols-[repeat(auto-fill,minmax(240px,1fr))] gap-x-4 my-4 gap-y-6">
                  <For each={activeListings()}>
                    {item => (
                      <StoreCard
                        active={selectedListing()?.id === item.id}
                        listing={item}
                        onSelect={() => setSelected(item)}
                      />
                    )}
                  </For>
                </div>

                <Show when={activeListings().length === 0 && loading()}>
                  <div class="grid grid-cols-[repeat(auto-fill,minmax(240px,1fr))] gap-x-4 my-4 gap-y-6">
                    <For each={[0, 1, 2]}>
                      {() => <StoreSkeleton />}
                    </For>
                  </div>
                </Show>

                <Show when={activeListings().length === 0 && !loading()}>
                  <div class="h-48 flex items-center justify-center text-sm text-muted-foreground">
                    No listings found.
                  </div>
                </Show>
              </div>

              <Show when={selectedListing()}>
                {item => <StoreDetails listing={item()} />}
              </Show>
            </div>
          </div>
        </Match>
      </Switch>
    </div>
  )
}

const TabButton: Component<{
  active: boolean
  count: number
  label: string
  onClick: () => void
}> = (props) => (
  <button
    type="button"
    onClick={props.onClick}
    class={`h-8 px-3 rounded text-sm font-medium transition-colors ${
      props.active
        ? 'bg-neutral-800 text-foreground shadow-sm'
        : 'text-muted-foreground hover:text-foreground'
    }`}
  >
    {props.label} <span class="text-xs opacity-70">{props.count}</span>
  </button>
)

const StoreCard: Component<{
  active: boolean
  listing: StoreListing
  onSelect: () => void
}> = (props) => {
  const [imageFailed, setImageFailed] = createSignal(false)
  const [avatarFailed, setAvatarFailed] = createSignal(false)

  const open = (url?: string) => (e: MouseEvent) => {
    e.preventDefault()
    e.stopPropagation()
    if (url) Shell.openLink(url)
  }

  const primaryAsset = createMemo(() => props.listing.assets[0])

  return (
    <div
      role="button"
      tabIndex={0}
      onClick={(event) => {
        event.stopPropagation()
        props.onSelect()
      }}
      onKeyDown={(event) => {
        if (event.key === 'Enter' || event.key === ' ') {
          event.stopPropagation()
          props.onSelect()
        }
      }}
      class={`text-left flex flex-col overflow-hidden shadow-md rounded-md border-solid bg-card border-[1px] hover:border-neutral-400 min-h-[360px] ${
        props.active ? 'border-neutral-300' : 'border-neutral-600'
      }`}
    >
      <div class="aspect-[16/10] bg-neutral-950 border-b border-neutral-700 overflow-hidden">
        <Show
          when={props.listing.image && !imageFailed()}
          fallback={
            <div class="h-full w-full grid place-items-center bg-neutral-900 text-xs text-muted-foreground">
              No preview
            </div>
          }
        >
          <img
            src={props.listing.image}
            alt=""
            class="h-full w-full object-contain bg-neutral-950"
            onError={() => setImageFailed(true)}
          />
        </Show>
      </div>

      <div class="flex flex-1 flex-col p-3 gap-2">
        <div class="flex items-start justify-between gap-2">
          <h3 class="font-semibold leading-5 text-base line-clamp-2 min-h-10">
            {props.listing.name}
          </h3>
          <button
            type="button"
            onClick={open(props.listing.repo ?? props.listing.discordUrl)}
            class="text-foreground/60 hover:text-foreground p-1 -m-1 shrink-0"
            tabIndex={-1}
            title={props.listing.repo ?? props.listing.discordUrl}
          >
            <LinkIcon size={14} />
          </button>
        </div>

        <Show
          when={props.listing.enriched}
          fallback={
            <div class="space-y-2 min-h-[60px] pt-1">
              <div class="h-3 rounded bg-neutral-800 animate-pulse" />
              <div class="h-3 w-5/6 rounded bg-neutral-800 animate-pulse" />
              <div class="h-3 w-2/3 rounded bg-neutral-800 animate-pulse" />
            </div>
          }
        >
          <p class="text-sm leading-5 text-muted-foreground line-clamp-3 min-h-[60px]">
            {props.listing.description}
          </p>
        </Show>

        <div class="flex flex-wrap gap-1 min-h-6">
          <For each={props.listing.tags.slice(0, 4)}>
            {tag => (
              <span class="rounded bg-neutral-800 px-2 py-0.5 text-[11px] text-muted-foreground">
                {tag}
              </span>
            )}
          </For>
        </div>

        <div class="mt-auto flex items-center justify-between gap-3 pt-2 border-t border-foreground/5">
          <div class="flex items-center gap-2 text-sm text-muted-foreground min-w-0">
            <Show when={props.listing.author.avatar && !avatarFailed()}>
              <img
                src={props.listing.author.avatar}
                alt=""
                class="size-4 rounded-full bg-neutral-700 shrink-0"
                onError={() => setAvatarFailed(true)}
              />
            </Show>
            <span class="truncate">{props.listing.author.name}</span>
          </div>

          <div class="flex items-center gap-2 shrink-0">
            <Show when={props.listing.releaseTag}>
              <button
                type="button"
                onClick={open(props.listing.releaseUrl)}
                class="text-xs text-muted-foreground hover:text-foreground"
                title={props.listing.releaseName ?? props.listing.releaseTag}
              >
                {props.listing.releaseTag}
              </button>
            </Show>
            <Show when={primaryAsset()}>
              {asset => (
                <button
                  type="button"
                  onClick={open(asset().downloadUrl)}
                  class="h-7 rounded bg-neutral-800 px-2 text-xs text-foreground hover:bg-neutral-700"
                  title={asset().name}
                >
                  Download
                </button>
              )}
            </Show>
          </div>
        </div>
      </div>
    </div>
  )
}

const StoreSkeleton: Component = () => (
  <div class="min-h-[360px] overflow-hidden rounded-md border border-neutral-700 bg-card">
    <div class="aspect-[16/10] bg-neutral-900 animate-pulse" />
    <div class="p-3 space-y-3">
      <div class="h-5 w-3/4 rounded bg-neutral-800 animate-pulse" />
      <div class="space-y-2">
        <div class="h-3 rounded bg-neutral-800 animate-pulse" />
        <div class="h-3 w-5/6 rounded bg-neutral-800 animate-pulse" />
        <div class="h-3 w-2/3 rounded bg-neutral-800 animate-pulse" />
      </div>
    </div>
  </div>
)

const StoreDetails: Component<{ listing: StoreListing }> = (props) => {
  const primaryAsset = createMemo(() => props.listing.assets[0])

  const open = (url?: string) => () => {
    if (url) Shell.openLink(url)
  }

  return (
    <aside
      class="sticky top-4 mt-4 max-h-[calc(100vh-96px)] overflow-auto rounded-md border border-neutral-700 bg-card animate-[store-panel-in_160ms_ease-out]"
      onClick={(event) => event.stopPropagation()}
    >
      <Show when={props.listing.image}>
        <div class="aspect-[16/10] border-b border-neutral-700 bg-neutral-950">
          <img src={props.listing.image} alt="" class="h-full w-full object-contain" />
        </div>
      </Show>

      <div class="p-4 space-y-4">
        <div>
          <h2 class="text-lg font-semibold leading-6">{props.listing.name}</h2>
          <p class="mt-1 text-sm text-muted-foreground">{props.listing.description}</p>
        </div>

        <div class="flex flex-wrap gap-1">
          <For each={props.listing.tags}>
            {tag => <span class="rounded bg-neutral-800 px-2 py-0.5 text-[11px] text-muted-foreground">{tag}</span>}
          </For>
        </div>

        <div class="grid grid-cols-2 gap-2">
          <Show when={props.listing.repo}>
            <button type="button" onClick={open(props.listing.repo)} class="h-8 rounded bg-neutral-800 text-sm hover:bg-neutral-700">
              Repository
            </button>
          </Show>
          <button type="button" onClick={open(props.listing.discordUrl)} class="h-8 rounded bg-neutral-800 text-sm hover:bg-neutral-700">
            Forum Post
          </button>
          <Show when={props.listing.releaseUrl}>
            <button type="button" onClick={open(props.listing.releaseUrl)} class="h-8 rounded bg-neutral-800 text-sm hover:bg-neutral-700">
              Release
            </button>
          </Show>
          <Show when={primaryAsset()}>
            {asset => (
              <button type="button" onClick={open(asset().downloadUrl)} class="h-8 rounded bg-neutral-200 text-sm text-neutral-950 hover:bg-neutral-300">
                Download
              </button>
            )}
          </Show>
        </div>

        <Show when={props.listing.releaseTag}>
          <div class="text-xs text-muted-foreground">
            Latest: <span class="text-foreground">{props.listing.releaseTag}</span>
          </div>
        </Show>

        <Show when={props.listing.details}>
          <div class="border-t border-neutral-700 pt-3">
            <h3 class="mb-2 text-sm font-medium">Details</h3>
            <p class="whitespace-pre-wrap text-sm leading-5 text-muted-foreground">
              {truncate(props.listing.details!, 1800)}
            </p>
          </div>
        </Show>
      </div>
    </aside>
  )
}

function truncate(value: string, max: number): string {
  if (value.length <= max) return value
  return `${value.slice(0, max).trim()}...`
}

function upsertListing(items: StoreListing[], listing: StoreListing): StoreListing[] {
  const index = items.findIndex(item => item.id === listing.id)
  if (index < 0) return [...items, listing]

  const next = [...items]
  next[index] = listing
  return next
}
