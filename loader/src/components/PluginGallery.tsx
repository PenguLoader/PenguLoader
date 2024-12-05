import {
  Component,
  For,
  createSignal,
  onMount,
  Switch,
  Match,
  Show,
  createEffect,
} from "solid-js";
import { type PluginInfo, PluginManager } from "../lib/plugins";
import { LoaderIcon, ReloadIcon, StoreIcon } from "./Icons";
import { Checkbox } from "./ui";
import { useConfig } from "~/lib/config";
import { useRoot } from "~/lib/root";
import { useI18n } from "~/lib/i18n";

const PluginCard: Component<PluginInfo> = (props) => {
  const [enabled, setEnabled] = createSignal(
    PluginManager.isEnabled(props.hash)
  );
  const toggle = () => {
    PluginManager.toggleState(props.hash).then(setEnabled);
  };
  return (
    <label
      draggable="false"
      class="flex flex-col gap-2 overflow-hidden shadow-md rounded-md border-solid bg-card border-[1px] border-neutral-600 hover:border-neutral-400"
    >
      {/* <div class="aspect-video relative overflow-hidden cursor-pointer aria-[disabled=true]:grayscale" aria-disabled={!enabled()}>
        <a href="/overlays/lol?select=arenaAugments">
          <img src="..." alt="" />
        </a>
      </div> */}
      <div class="flex flex-col p-3 gap-2 items-stretch">
        <div class="flex items-center space-x-2">
          <Checkbox checked={enabled()} onClick={toggle} />
          <h3 class="font-semibold leading-7 text-base text-ellipsis whitespace-nowrap overflow-hidden">
            {props.name}
          </h3>
        </div>
        {/* <ul class="flex items-center gap-1 flex-1">
          <li class="rounded-sm px-1 bg-slate-300 leading-3">
            <span class="text-xs leading-5 font-semibold">Utility</span>
          </li>
        </ul> */}
        <div class="text-sm leading-5 text-muted-foreground break-words">
          @plugins/{props.path}
        </div>
      </div>
    </label>
  );
};

export const PluginGallery: Component = () => {
  const i18n = useI18n();
  const config = useConfig();
  const { setStore } = useRoot();

  const [loading, setLoading] = createSignal(false);
  const [plugins, setPlugins] = createSignal(Array<PluginInfo>(), {
    equals: false,
  });

  const revealPlugins = () => {
    PluginManager.openFolder();
  };

  const reload = () => {
    setPlugins([]);
    setLoading(true);

    Promise.all([
      PluginManager.getPlugins()
        .then(setPlugins)
        .catch(() => {}),
      new Promise((r) => setTimeout(r, 500)),
    ]).finally(() => setLoading(false));
  };

  onMount(reload);
  createEffect(() => {
    // watch the dir changes
    config.app.plugins_dir();
    reload();
  });

  return (
    <div class="h-full">
      <Switch>
        <Match when={loading()}>
          <div class="text-accent-foreground m-auto flex flex-col items-center justify-center gap-2 h-full">
            <LoaderIcon class="animate-spin" />
            <p>{i18n.t("loading")}</p>
          </div>
        </Match>
        <Match when={!loading()}>
          <div class="grid p-4">
            <h1 class="text-foreground/80 text-sm">
              {i18n.t("installed_plugins")} ({plugins().length})
            </h1>
            <Show
              when={plugins().length > 0}
              fallback={
                <h3 class="text-center my-8 w-full">{i18n.t("no_plugins")}</h3>
              }
            >
              <div class="grid grid-cols-3 gap-x-4 my-4 gap-y-6">
                <For each={plugins()}>
                  {(plugin) => <PluginCard {...plugin} />}
                </For>
              </div>
            </Show>
            <div class="flex justify-evenly items-center w-full py-8">
              <div class="flex flex-col items-center space-y-4">
                <p class="text-sm text-secondary-foreground/70">
                  {i18n.t("plugins_are_missing")}
                </p>
                <div class="flex gap-1">
                  <button
                    class="inline-flex gap-1 items-center text-sm border border-foreground/10 rounded-sm px-3 py-1 hover:bg-foreground hover:text-background"
                    tabIndex={-1}
                    onClick={reload}
                  >
                    <ReloadIcon size={14} /> {i18n.t("reload")}
                  </button>
                  <button
                    class="inline-flex gap-1 items-center text-sm border border-foreground/10 rounded-sm px-3 py-1 hover:bg-foreground hover:text-background"
                    tabIndex={-1}
                    onClick={revealPlugins}
                  >
                    {i18n.t("open_folder")}
                  </button>
                </div>
              </div>
              <div class="flex flex-col items-center space-y-4">
                <p class="text-sm text-secondary-foreground/70">
                  {i18n.t("more_plugins")}
                </p>
                <button
                  class="inline-flex gap-1 items-center text-sm border border-foreground/10 rounded-sm px-3 py-1 hover:bg-foreground hover:text-background"
                  tabIndex={-1}
                  onClick={() => setStore(true)}
                >
                  <StoreIcon size={14} /> {i18n.t("get_in_store")}
                </button>
              </div>
            </div>
          </div>
        </Match>
      </Switch>
    </div>
  );
};
