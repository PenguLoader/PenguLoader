import { Component, createMemo, createSignal, For, Match, onMount, Show, Switch } from 'solid-js'
import { StoreKind, StoreListing, StoreManager } from '~/lib/store'
import { pengu, StoreInstallResult } from '~/lib/pengu'
import { Shell } from '~/lib/shell'
import { LinkIcon, LoaderIcon } from './Icons'

type InstallState = {
  installed: boolean
  busy?: boolean
  folderName?: string
  message?: string
  error?: string
}

type MarkdownBlock =
  | { type: 'paragraph'; text: string }
  | { type: 'heading'; level: number; text: string }
  | { type: 'list'; ordered: boolean; items: string[] }
  | { type: 'quote'; text: string }
  | { type: 'code'; lang?: string; code: string }

type InlineNode =
  | { type: 'text'; text: string }
  | { type: 'link'; text: string; href: string }
  | { type: 'code'; text: string }
  | { type: 'strong'; text: string }
  | { type: 'em'; text: string }

export const PluginStore: Component = () => {
  const [loading, setLoading] = createSignal(true)
  const [error, setError] = createSignal<string | null>(null)
  const [activeTab, setActiveTab] = createSignal<StoreKind>('plugins')
  const [selected, setSelected] = createSignal<StoreListing | null>(null)
  const [manifestStatus, setManifestStatus] = createSignal<string | null>(null)
  const [listings, setListings] = createSignal<Record<StoreKind, StoreListing[]>>({
    plugins: [],
    themes: [],
  })
  const [installStates, setInstallStates] = createSignal<Record<string, InstallState>>({})

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
        void syncInstallState(listing, setInstallStates)
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
          <div class="h-full overflow-auto p-4">
            <div class="flex items-center justify-between gap-4">
              <div class="flex items-center gap-2">
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
                <button
                  type="button"
                  class="h-9 rounded-md border border-neutral-700 bg-neutral-900 px-3 text-sm text-foreground hover:bg-neutral-800"
                  onClick={() => installManifestFromPrompt(setManifestStatus)}
                >
                  Install from GitHub
                </button>
              </div>
              <Show
                when={loading()}
                fallback={<p class="text-xs text-muted-foreground">{manifestStatus() ?? 'Discord forum listings enriched with GitHub releases'}</p>}
              >
                <div class="flex items-center gap-2 text-xs text-muted-foreground">
                  <LoaderIcon class="animate-spin" size={13} />
                  <span>Loading more listings...</span>
                </div>
              </Show>
            </div>

            <div
              class="grid gap-x-4 my-4 gap-y-6"
              style="grid-template-columns: repeat(auto-fill, minmax(max(270px, calc((100% - 48px) / 4)), 1fr))"
            >
              <For each={activeListings()}>
                {item => (
                  <StoreCard
                    active={selectedListing()?.id === item.id}
                    installState={installStates()[item.id]}
                    listing={item}
                    onInstall={() => installListing(item, installStates()[item.id], setInstallStates)}
                    onSelect={() => setSelected(item)}
                  />
                )}
              </For>
            </div>

            <Show when={activeListings().length === 0 && loading()}>
              <div
                class="grid gap-x-4 my-4 gap-y-6"
                style="grid-template-columns: repeat(auto-fill, minmax(max(270px, calc((100% - 48px) / 4)), 1fr))"
              >
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
            {item => (
              <StoreDetails
                installState={installStates()[item().id]}
                listing={item()}
                onClose={() => setSelected(null)}
                onInstall={() => installListing(item(), installStates()[item().id], setInstallStates)}
              />
            )}
          </Show>
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
  installState?: InstallState
  listing: StoreListing
  onInstall: () => void
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
  const supported = createMemo(() => isSupportedAsset(primaryAsset()?.name))

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
      class={`relative text-left flex flex-col overflow-hidden shadow-md rounded-md border-solid bg-card border-[1px] hover:border-neutral-400 min-h-[360px] ${
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
            <Show when={(props.listing.upvotes ?? 0) > 0}>
              <span
                class="flex items-center gap-0.5 text-xs text-muted-foreground"
                title={`${props.listing.upvotes} upvotes`}
              >
                <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round">
                  <path d="M12 19V5M5 12l7-7 7 7"/>
                </svg>
                <span class="tabular-nums font-medium">{props.listing.upvotes}</span>
              </span>
            </Show>
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
                <InstallButton
                  assetName={asset().name}
                  compact
                  installState={props.installState}
                  onInstall={props.onInstall}
                  supported={supported()}
                />
              )}
            </Show>
          </div>
        </div>
        <InstallStatus state={props.installState} />
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

const StoreDetails: Component<{
  installState?: InstallState
  listing: StoreListing
  onClose: () => void
  onInstall: () => void
}> = (props) => {
  const primaryAsset = createMemo(() => props.listing.assets[0])
  const supported = createMemo(() => isSupportedAsset(primaryAsset()?.name))

  const open = (url?: string) => () => {
    if (url) Shell.openLink(url)
  }

  return (
    <div
      class="fixed inset-0 z-50 flex items-center justify-center p-8 animate-[store-backdrop-in_200ms_ease-out]"
      style="background: rgba(0,0,0,0.6); backdrop-filter: blur(4px)"
      onClick={() => props.onClose()}
      onKeyDown={(e) => { if (e.key === 'Escape') props.onClose() }}
    >
      <aside
        class="w-full max-w-3xl max-h-[calc(100vh-96px)] overflow-auto rounded-lg border border-neutral-700 bg-card shadow-2xl animate-[store-panel-in_250ms_ease-out]"
      onClick={(event) => event.stopPropagation()}
    >
      <Show when={props.listing.image}>
        <div class="aspect-[16/10] border-b border-neutral-700 bg-neutral-950">
          <img src={props.listing.image} alt="" class="h-full w-full object-contain" />
        </div>
      </Show>

      <div class="p-4 space-y-4">
        <div>
          <div class="flex items-start justify-between gap-3">
            <h2 class="text-lg font-semibold leading-6">{props.listing.name}</h2>
            <VotePill count={props.listing.upvotes ?? 0} />
          </div>
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
              <InstallButton
                assetName={asset().name}
                installState={props.installState}
                onInstall={props.onInstall}
                supported={supported()}
              />
            )}
          </Show>
        </div>
        <InstallStatus state={props.installState} />

        <Show when={props.listing.releaseTag}>
          <div class="text-xs text-muted-foreground">
            Latest: <span class="text-foreground">{props.listing.releaseTag}</span>
          </div>
        </Show>

        <Show when={props.listing.details}>
          <div class="border-t border-neutral-700 pt-3">
            <h3 class="mb-2 text-sm font-medium">Details</h3>
            <MarkdownDetails value={props.listing.details!} />
          </div>
        </Show>
      </div>
    </aside>
    </div>
  )
}



const VotePill: Component<{ count: number }> = (props) => (
  <Show when={props.count > 0}>
    <div
      class="shrink-0 rounded-full border border-neutral-600 bg-neutral-900 px-2.5 py-1 text-xs text-muted-foreground"
      title={`${props.count} upvotes`}
    >
      <span class="font-semibold text-foreground tabular-nums">{props.count}</span> votes
    </div>
  </Show>
)

const MarkdownDetails: Component<{ value: string }> = (props) => {
  const blocks = createMemo(() => parseMarkdownBlocks(props.value))

  return (
    <div class="space-y-3 text-sm leading-5 text-muted-foreground">
      <For each={blocks()}>
        {block => (
          <Switch>
            <Match when={block.type === 'code'}>
              <div class="overflow-hidden rounded-md border border-neutral-700 bg-neutral-950">
                <Show when={(block as Extract<MarkdownBlock, { type: 'code' }>).lang}>
                  {lang => (
                    <div class="border-b border-neutral-800 px-3 py-1 text-[11px] uppercase tracking-normal text-muted-foreground">
                      {lang()}
                    </div>
                  )}
                </Show>
                <pre class="max-h-80 overflow-auto p-3 text-xs leading-5 text-neutral-200">
                  <code>{(block as Extract<MarkdownBlock, { type: 'code' }>).code}</code>
                </pre>
              </div>
            </Match>
            <Match when={block.type === 'paragraph'}>
              <p class="whitespace-pre-wrap">
                <MarkdownInline value={(block as Extract<MarkdownBlock, { type: 'paragraph' }>).text} />
              </p>
            </Match>
            <Match when={block.type === 'heading'}>
              {(() => {
                const heading = block as Extract<MarkdownBlock, { type: 'heading' }>
                const className = heading.level === 1
                  ? 'text-lg font-semibold leading-6 text-foreground'
                  : heading.level === 2
                    ? 'text-base font-semibold leading-6 text-foreground'
                    : 'text-sm font-semibold leading-5 text-foreground'
                return (
                  <div class={className}>
                    <MarkdownInline value={heading.text} />
                  </div>
                )
              })()}
            </Match>
            <Match when={block.type === 'list'}>
              {(() => {
                const list = block as Extract<MarkdownBlock, { type: 'list' }>
                const items = (
                  <For each={list.items}>
                    {item => (
                      <li>
                        <MarkdownInline value={item} />
                      </li>
                    )}
                  </For>
                )
                return list.ordered
                  ? <ol class="list-decimal space-y-1 pl-5">{items}</ol>
                  : <ul class="list-disc space-y-1 pl-5">{items}</ul>
              })()}
            </Match>
            <Match when={block.type === 'quote'}>
              <blockquote class="border-l border-neutral-600 pl-3 text-neutral-300">
                <MarkdownInline value={(block as Extract<MarkdownBlock, { type: 'quote' }>).text} />
              </blockquote>
            </Match>
          </Switch>
        )}
      </For>
    </div>
  )
}

const MarkdownInline: Component<{ value: string }> = (props) => {
  const nodes = createMemo(() => parseInlineMarkdown(props.value))

  return (
    <>
      <For each={nodes()}>
        {node => (
          <Switch>
            <Match when={node.type === 'link'}>
              {(() => {
                const link = node as Extract<InlineNode, { type: 'link' }>
                return (
                  <button
                    type="button"
                    class="text-neutral-200 underline decoration-neutral-500 underline-offset-2 hover:text-foreground"
                    onClick={(event) => {
                      event.stopPropagation()
                      Shell.openLink(link.href)
                    }}
                  >
                    {link.text}
                  </button>
                )
              })()}
            </Match>
            <Match when={node.type === 'code'}>
              <code class="rounded bg-neutral-900 px-1 py-0.5 text-[0.9em] text-neutral-200">
                {(node as Extract<InlineNode, { type: 'code' }>).text}
              </code>
            </Match>
            <Match when={node.type === 'strong'}>
              <strong class="font-semibold text-neutral-200">
                {(node as Extract<InlineNode, { type: 'strong' }>).text}
              </strong>
            </Match>
            <Match when={node.type === 'em'}>
              <em class="text-neutral-200">
                {(node as Extract<InlineNode, { type: 'em' }>).text}
              </em>
            </Match>
            <Match when={node.type === 'text'}>
              {(node as Extract<InlineNode, { type: 'text' }>).text}
            </Match>
          </Switch>
        )}
      </For>
    </>
  )
}

const InstallButton: Component<{
  assetName: string
  compact?: boolean
  installState?: InstallState
  onInstall: () => void
  supported: boolean
}> = (props) => {
  const label = createMemo(() => {
    if (!props.supported) return 'Unsupported'
    if (props.installState?.busy) return 'Installing'
    if (props.installState?.installed) return 'Installed'
    return 'Install'
  })

  return (
    <button
      type="button"
      disabled={!props.supported || props.installState?.busy}
      onClick={(event) => {
        event.preventDefault()
        event.stopPropagation()
        props.onInstall()
      }}
      class={`${props.compact ? 'h-7 px-2 text-xs' : 'h-8 text-sm'} rounded ${
        props.supported
          ? props.installState?.installed
            ? 'bg-emerald-900/50 text-emerald-100 hover:bg-emerald-800/60'
            : 'bg-neutral-200 text-neutral-950 hover:bg-neutral-300'
          : 'cursor-not-allowed bg-neutral-800 text-muted-foreground'
      } disabled:opacity-70`}
      title={props.supported ? props.assetName : 'Only .js and .zip assets can be installed'}
    >
      {props.installState?.busy && <LoaderIcon class="mr-1 inline animate-spin align-[-2px]" size={12} />}
      {label()}
    </button>
  )
}

const InstallStatus: Component<{ state?: InstallState }> = (props) => (
  <Show when={props.state?.message || props.state?.error}>
    <p class={`text-xs ${props.state?.error ? 'text-destructive' : 'text-muted-foreground'}`}>
      {props.state?.error ?? props.state?.message}
    </p>
  </Show>
)

async function syncInstallState(
  listing: StoreListing,
  setInstallStates: (fn: (current: Record<string, InstallState>) => Record<string, InstallState>) => void,
) {
  try {
    const result = await pengu.plugins.checkStoreInstall({
      listingName: listing.name,
      repo: listing.repo,
    })
    setInstallStates(current => ({
      ...current,
      [listing.id]: stateFromResult(result, current[listing.id]?.message),
    }))
  } catch {
    // Store install state is local convenience; registry rendering should not fail if it is unavailable.
  }
}

async function installManifestFromPrompt(setStatus: (value: string | null) => void) {
  const repo = prompt('GitHub repo URL or owner/repo')
  if (!repo?.trim()) return

  const runInstall = (replace: boolean) => pengu.plugins.installManifest({
    repo: repo.trim(),
    replace,
  })

  try {
    setStatus('Installing manifest...')
    let result = await runInstall(false)
    if (result.conflict) {
      if (!confirm(`A folder named ${result.folderName ?? repo} already exists. Replace it?`)) {
        setStatus('Manifest install cancelled.')
        return
      }
      setStatus('Replacing manifest install...')
      result = await runInstall(true)
    }

    setStatus(result.ok
      ? `Installed ${result.folderName ?? 'plugin'} from manifest.`
      : `Manifest install failed: ${result.error ?? 'Unknown error.'}`)
  } catch (e) {
    setStatus(`Manifest install failed: ${e instanceof Error ? e.message : String(e)}`)
  }
}

async function installListing(
  listing: StoreListing,
  state: InstallState | undefined,
  setInstallStates: (fn: (current: Record<string, InstallState>) => Record<string, InstallState>) => void,
) {
  const asset = listing.assets[0]
  if (!asset || !isSupportedAsset(asset.name)) {
    setInstallStates(current => ({
      ...current,
      [listing.id]: {
        ...current[listing.id],
        installed: Boolean(current[listing.id]?.installed),
        error: 'Only .js and .zip release assets can be installed.',
      },
    }))
    return
  }

  const replace = Boolean(state?.installed)
  if (replace && !confirm(`Replace the installed copy of ${listing.name}?`))
    return

  const runInstall = async (allowReplace: boolean) => {
    setInstallStates(current => ({
      ...current,
      [listing.id]: {
        ...current[listing.id],
        busy: true,
        installed: Boolean(current[listing.id]?.installed),
        message: 'Downloading and installing...',
        error: undefined,
      },
    }))

    return pengu.plugins.installStoreAsset({
      listingId: listing.id,
      listingName: listing.name,
      kind: listing.kind,
      repo: listing.repo,
      assetName: asset.name,
      downloadUrl: asset.downloadUrl,
      contentType: asset.contentType,
      replace: allowReplace,
    })
  }

  try {
    let result = await runInstall(replace)
    if (result.conflict) {
      setInstallStates(current => ({
        ...current,
        [listing.id]: {
          ...stateFromResult(result),
          message: undefined,
          error: result.error ?? 'A plugin folder with that name already exists.',
        },
      }))
      if (!confirm(`A folder named ${result.folderName ?? listing.name} already exists. Replace it?`))
        return
      result = await runInstall(true)
    }

    setInstallStates(current => ({
      ...current,
      [listing.id]: result.ok
        ? stateFromResult(result, 'Installed successfully.')
        : {
          ...stateFromResult(result),
          error: result.error ?? 'Install failed.',
        },
    }))
  } catch (e) {
    setInstallStates(current => ({
      ...current,
      [listing.id]: {
        ...current[listing.id],
        busy: false,
        installed: Boolean(current[listing.id]?.installed),
        error: e instanceof Error ? e.message : String(e),
      },
    }))
  }
}

function stateFromResult(result: StoreInstallResult, message?: string): InstallState {
  return {
    installed: result.alreadyInstalled,
    busy: false,
    folderName: result.folderName,
    message,
    error: result.ok ? undefined : result.error,
  }
}

function isSupportedAsset(assetName?: string): boolean {
  return Boolean(assetName && /\.(js|zip)$/i.test(assetName))
}

function parseMarkdownBlocks(value: string): MarkdownBlock[] {
  const blocks: MarkdownBlock[] = []
  const lines = value.replace(/\r\n/g, '\n').split('\n')
  let paragraph: string[] = []
  let listItems: string[] = []
  let listOrdered = false
  let code: string[] | null = null
  let codeLang: string | undefined

  const flushParagraph = () => {
    const text = paragraph.join('\n').trim()
    if (text) blocks.push({ type: 'paragraph', text })
    paragraph = []
  }

  const flushList = () => {
    if (listItems.length) blocks.push({ type: 'list', ordered: listOrdered, items: listItems })
    listItems = []
    listOrdered = false
  }

  const flushCode = () => {
    blocks.push({ type: 'code', lang: codeLang, code: (code ?? []).join('\n').trimEnd() })
    code = null
    codeLang = undefined
  }

  for (const line of lines) {
    const fence = line.match(/^```(\S+)?(?:\s+(.*))?$/)
    if (fence) {
      if (code) {
        flushCode()
      } else {
        flushParagraph()
        flushList()
        codeLang = fence[1]
        code = []
        if (fence[2]) code.push(fence[2])
      }
      continue
    }

    if (code) {
      code.push(line)
      continue
    }

    const heading = line.match(/^(#{1,6})\s+(.+)$/)
    if (heading) {
      flushParagraph()
      flushList()
      blocks.push({ type: 'heading', level: heading[1].length, text: heading[2].trim() })
      continue
    }

    const quote = line.match(/^>\s?(.*)$/)
    if (quote) {
      flushParagraph()
      flushList()
      blocks.push({ type: 'quote', text: quote[1].trim() })
      continue
    }

    const unordered = line.match(/^\s*[-*]\s+(.+)$/)
    const ordered = line.match(/^\s*\d+[.)]\s+(.+)$/)
    if (unordered || ordered) {
      flushParagraph()
      const nextOrdered = Boolean(ordered)
      if (listItems.length && listOrdered !== nextOrdered) flushList()
      listOrdered = nextOrdered
      listItems.push((ordered?.[1] ?? unordered?.[1] ?? '').trim())
      continue
    }

    if (line.trim()) {
      flushList()
      paragraph.push(line)
    } else {
      flushParagraph()
      flushList()
    }
  }

  if (code) flushCode()
  flushParagraph()
  flushList()
  return blocks
}

function parseInlineMarkdown(value: string): InlineNode[] {
  const nodes: InlineNode[] = []
  const pattern = /(\[([^\]]+)\]\((https?:\/\/[^)\s]+)\)|`([^`]+)`|\*\*([^*]+)\*\*|\*([^*]+)\*)/g
  let last = 0
  let match: RegExpExecArray | null

  while ((match = pattern.exec(value)) !== null) {
    if (match.index > last) nodes.push({ type: 'text', text: value.slice(last, match.index) })

    if (match[2] && match[3]) nodes.push({ type: 'link', text: match[2], href: match[3] })
    else if (match[4]) nodes.push({ type: 'code', text: match[4] })
    else if (match[5]) nodes.push({ type: 'strong', text: match[5] })
    else if (match[6]) nodes.push({ type: 'em', text: match[6] })

    last = pattern.lastIndex
  }

  if (last < value.length) nodes.push({ type: 'text', text: value.slice(last) })
  return nodes
}

function upsertListing(items: StoreListing[], listing: StoreListing): StoreListing[] {
  const index = items.findIndex(item => item.id === listing.id)
  if (index < 0) return [...items, listing]

  const next = [...items]
  next[index] = listing
  return next
}
