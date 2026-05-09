import { Component, For, createSignal, onMount, Switch, Match, Show, createEffect } from 'solid-js'
import { type PluginInfo, PluginManager } from '../lib/plugins'
import { LinkIcon, LoaderIcon, ReloadIcon, StoreIcon } from './Icons'
import { Checkbox } from './ui'
import { useConfig } from '~/lib/config'
import { useRoot } from '~/lib/root'
import { Shell } from '~/lib/shell'

/**
 * Derive a GitHub avatar URL from the @author handle our discovery captured.
 * Returns null when the handle isn't a plausible GitHub username — e.g. it
 * contains '#' (Discord-tagged), so https://github.com/<handle>.png would 404.
 * Author text always shows; the avatar is just the optional visual.
 */
function githubAvatar(author: string | undefined): string | null {
  if (!author) return null
  if (author.includes('#')) return null
  const handle = author.startsWith('@') ? author.slice(1) : author
  if (!handle) return null
  return `https://github.com/${encodeURIComponent(handle)}.png?size=32`
}

const PluginCard: Component<PluginInfo> = (props) => {
  const [enabled, setEnabled] = createSignal(props.enabled)
  const [avatarFailed, setAvatarFailed] = createSignal(false)
  const toggle = () => {
    PluginManager.toggleState(props.path).then(setEnabled)
  }
  const openLink = (e: MouseEvent) => {
    e.preventDefault()
    e.stopPropagation()
    if (props.link) Shell.openLink(props.link)
  }

  const avatarSrc = () => githubAvatar(props.author)

  return (
    <label draggable="false" class="flex flex-col gap-2 overflow-hidden shadow-md rounded-md border-solid bg-card border-[1px] border-neutral-600 hover:border-neutral-400">
      <div class="flex flex-col p-3 gap-2 items-stretch">
        <div class="flex items-center space-x-2">
          <Checkbox checked={enabled()} onClick={toggle} />
          <h3 class="font-semibold leading-7 text-base text-ellipsis whitespace-nowrap overflow-hidden">{props.name}</h3>
        </div>
        <Show when={props.description}>
          <p class="text-sm leading-5 text-muted-foreground line-clamp-2">{props.description}</p>
        </Show>
        <p class="text-xs text-muted-foreground/60 break-all">@plugins/{props.path}</p>
        <Show when={props.author || props.link}>
          <div class="flex items-center justify-between pt-1 mt-1 border-t border-foreground/5">
            <Show when={props.author} fallback={<span />}>
              <div class="flex items-center gap-2 text-sm text-muted-foreground min-w-0">
                <Show when={avatarSrc() && !avatarFailed()}>
                  <img
                    src={avatarSrc()!}
                    alt=""
                    class="size-4 rounded-full bg-neutral-700 shrink-0"
                    onError={() => setAvatarFailed(true)}
                  />
                </Show>
                <span class="truncate">{props.author}</span>
              </div>
            </Show>
            <Show when={props.link}>
              <button
                type="button"
                onClick={openLink}
                class="text-foreground/60 hover:text-foreground p-1 -m-1 shrink-0"
                tabIndex={-1}
                title={props.link}
              >
                <LinkIcon size={14} />
              </button>
            </Show>
          </div>
        </Show>
      </div>
    </label>
  )
}

export const PluginGallery: Component = () => {

  const config = useConfig()
  const { setStore } = useRoot()

  const [loading, setLoading] = createSignal(false)
  const [plugins, setPlugins] = createSignal(Array<PluginInfo>(), { equals: false })

  const revealPlugins = () => {
    PluginManager.openFolder()
  }

  const reload = () => {
    setPlugins([])
    setLoading(true)

    Promise.all([
      PluginManager.getPlugins()
        .then(setPlugins)
        .catch(() => { }),
      new Promise((r) => setTimeout(r, 500))
    ])
      .finally(() => setLoading(false))
  }

  onMount(reload)
  createEffect(() => {
    // watch the dir changes
    config.app.plugins_dir()
    reload()
  })

  return (
    <div class="h-full">
      <Switch>
        <Match when={loading()}>
          <div class="text-accent-foreground m-auto flex flex-col items-center justify-center gap-2 h-full">
            <LoaderIcon class="animate-spin" />
            <p>Loading...</p>
          </div>
        </Match>
        <Match when={!loading()}>
          {/* The cards block scrolls in MainPage's overflow-y-auto container.
              Padding-bottom reserves room so the last row of cards doesn't
              hide behind the fixed footer below. */}
          <div class="grid p-4 pb-[35vh]">
            <h1 class="text-foreground/80 text-sm">Installed plugins ({plugins().length})</h1>
            <Show
              when={plugins().length > 0}
              fallback={<h3 class="text-center my-8 w-full">You have no plugins!</h3>}
            >
              <div class="grid grid-cols-3 gap-x-4 my-4 gap-y-6">
                <For each={plugins()}>
                  {plugin => <PluginCard {...plugin} />}
                </For>
              </div>
            </Show>
            <div class="flex justify-evenly items-center w-full pt-24">
              <div class="flex flex-col items-center space-y-4">
                <p class="text-sm text-secondary-foreground/70">Don't see your plugins?</p>
                <div class="flex gap-1">
                  <button
                    class="inline-flex gap-1 items-center text-sm border border-foreground/10 rounded-sm px-3 py-1 hover:bg-foreground hover:text-background"
                    tabIndex={-1}
                    onClick={reload}
                  >
                    <ReloadIcon size={14} /> Reload
                  </button>
                  <button
                    class="inline-flex gap-1 items-center text-sm border border-foreground/10 rounded-sm px-3 py-1 hover:bg-foreground hover:text-background"
                    tabIndex={-1}
                    onClick={revealPlugins}
                  >
                    Open folder
                  </button>
                </div>
              </div>
              <div class="flex flex-col items-center space-y-4">
                <p class="text-sm text-secondary-foreground/70">More plugins?</p>
                <button
                  class="inline-flex gap-1 items-center text-sm border border-foreground/10 rounded-sm px-3 py-1 hover:bg-foreground hover:text-background"
                  tabIndex={-1}
                  onClick={() => setStore(true)}
                >
                  <StoreIcon size={14} /> Get in Store
                </button>
              </div>
            </div>
          </div>
        </Match>
      </Switch>
    </div>
  )
}