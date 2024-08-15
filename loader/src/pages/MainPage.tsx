import { Component, Match, Show, Switch } from 'solid-js'
import { Activator } from '../components/Activator'
import { Settings } from '../components/settings'
import { PluginGallery } from '../components/PluginGallery'
import { PluginStore } from '../components/PluginStore'
import { useRoot } from '~/lib/root'

export const MainPage: Component = () => {

  const { isStore } = useRoot()

  return (
    <div class="flex flex-col flex-1 overflow-hidden">
      <div class="flex-1 overflow-y-auto container">
        <Switch>
          <Match when={isStore()} children={PluginStore} />
          <Match when={!isStore()} children={PluginGallery} />
        </Switch>
      </div>
      <Show when={!isStore()} children={Activator} />
      <Settings />
    </div>
  )
}