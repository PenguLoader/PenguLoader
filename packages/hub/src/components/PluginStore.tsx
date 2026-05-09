import { Component, createSignal, For, Match, onMount, Show, Switch } from 'solid-js'
import { StoreManager, type StorePlugin } from '~/lib/store'
import { Shell } from '~/lib/shell'
import { LinkIcon, LoaderIcon } from './Icons'

export const PluginStore: Component = () => {

  const [loading, setLoading] = createSignal(true)
  const [error, setError] = createSignal<string | null>(null)
  const [plugins, setPlugins] = createSignal<StorePlugin[]>([])

  onMount(async () => {
    try {
      const list = await StoreManager.fetchPlugins()
      setPlugins(list)
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e))
    } finally {
      setLoading(false)
    }
  })

  return (
    <div class="h-full">
      <Switch>
        <Match when={loading()}>
          <div class="text-accent-foreground m-auto flex flex-col items-center justify-center gap-2 h-full">
            <LoaderIcon class="animate-spin" />
            <p>Loading registry...</p>
          </div>
        </Match>
        <Match when={error()}>
          <div class="h-full flex flex-col items-center justify-center gap-2 text-center">
            <p class="text-destructive">Failed to fetch the plugin registry.</p>
            <p class="text-xs text-muted-foreground">{error()}</p>
          </div>
        </Match>
        <Match when={!loading() && !error()}>
          <div class="grid p-4">
            <h1 class="text-foreground/80 text-sm">Plugin Store ({plugins().length})</h1>
            <div class="grid grid-cols-3 gap-x-4 my-4 gap-y-6">
              <For each={plugins()}>
                {p => <StoreCard plugin={p} />}
              </For>
            </div>
          </div>
        </Match>
      </Switch>
    </div>
  )
}

const StoreCard: Component<{ plugin: StorePlugin }> = (props) => {
  const [iconFailed, setIconFailed] = createSignal(false)
  const [avatarFailed, setAvatarFailed] = createSignal(false)

  const openRepo = (e: MouseEvent) => {
    e.preventDefault()
    e.stopPropagation()
    Shell.openLink(props.plugin.repo)
  }

  return (
    <div class="flex flex-col gap-2 overflow-hidden shadow-md rounded-md border-solid bg-card border-[1px] border-neutral-600 hover:border-neutral-400">
      <div class="flex flex-col p-3 gap-2 items-stretch">
        <div class="flex items-center space-x-2">
          <Show when={!iconFailed()} fallback={<div class="size-4 shrink-0" />}>
            <img
              src={props.plugin.image}
              alt=""
              class="size-4 object-contain shrink-0"
              onError={() => setIconFailed(true)}
            />
          </Show>
          <h3 class="font-semibold leading-7 text-base text-ellipsis whitespace-nowrap overflow-hidden">
            {props.plugin.name}
          </h3>
        </div>
        <p class="text-sm leading-5 text-muted-foreground line-clamp-2">
          {props.plugin.description}
        </p>
        <div class="flex items-center justify-between pt-1 mt-1 border-t border-foreground/5">
          <div class="flex items-center gap-2 text-sm text-muted-foreground min-w-0">
            <Show when={!avatarFailed()}>
              <img
                src={`https://github.com/${props.plugin.author.github}.png?size=32`}
                alt=""
                class="size-4 rounded-full bg-neutral-700 shrink-0"
                onError={() => setAvatarFailed(true)}
              />
            </Show>
            <span class="truncate">{props.plugin.author.name}</span>
          </div>
          <button
            type="button"
            onClick={openRepo}
            class="text-foreground/60 hover:text-foreground p-1 -m-1 shrink-0"
            tabIndex={-1}
            title={props.plugin.repo}
          >
            <LinkIcon size={14} />
          </button>
        </div>
      </div>
    </div>
  )
}
