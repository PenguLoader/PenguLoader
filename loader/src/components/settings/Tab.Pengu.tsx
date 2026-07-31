import { Component, createSignal, onMount, Show } from 'solid-js'
import { dialog, invoke } from '@tauri-apps/api'
import { Config, useConfig } from '~/lib/config'
import { LeagueClient } from '~/lib/league-client'
import { CheckOption, OptionSet, RadioOption } from './templates'
import { ActivationMode, CoreModule } from '~/lib/core-module'
import { Startup } from '~/lib/startup'

const LaunchSettings: Component = () => {
  const [startup, setSatrtup] = createSignal(false)

  const toggleStartup = async () => {
    let enable = !await Startup.isEnabled()
    await Startup.setEnable(enable)
    setSatrtup(enable)
  }

  onMount(async () => {
    setSatrtup(await Startup.isEnabled())
  })

  return (
    <OptionSet name="Launch Settings">
      <CheckOption
        caption="Run on startup"
        message="Automatically run Pengu when your computer starts."
        checked={startup()}
        onClick={toggleStartup}
      />
    </OptionSet>
  )
}

export const TabPengu: Component = () => {

  const { app } = useConfig()
  const [switchingChannel, setSwitchingChannel] = createSignal(false)

  const switchToMain = async () => {
    if (switchingChannel()) return

    const confirmed = await dialog.ask(
      'Switch to the temporary Main-ready WPF build? Close League and Riot Client before continuing.',
      { title: 'Switch release channel', type: 'info' }
    )
    if (!confirmed) return

    setSwitchingChannel(true)
    try {
      await invoke('plugin:update_channel|switch_to_main')
    } catch (error) {
      setSwitchingChannel(false)
      await dialog.message(`Failed to switch release channel.\n${error}`, { type: 'error' })
    }
  }

  const changePluginsDir = async () => {
    const dir = await dialog.open({
      directory: true,
      defaultPath: Config.basePath(),
    })

    if (typeof dir === 'string') {
      await app.plugins_dir(dir)
    }
  }

  const setActivationMode = async (mode: ActivationMode) => {
    if (await CoreModule.isActivated()) {
      await dialog.message('Please deactivate Pengu before changing the activation mode.', { type: 'warning' })
    } else {
      await app.activation_mode(mode)
    }
  }

  const changeLeagueDir = async () => {
    const dir = await dialog.open({
      directory: true
    })

    if (typeof dir === 'string') {
      if (await LeagueClient.validateDir(dir)) {
        await app.league_dir(dir)
      } else {
        await dialog.message('Your selected path is not valid.', { type: 'warning' })
      }
    }
  }

  return (
    <div class="space-y-4">

      <Show when={!window.isMac}>
        <OptionSet name="Release Channel" disabled={switchingChannel()}>
          <RadioOption
            caption="Main"
            message="Temporary WPF Main-ready build."
            checked={false}
            onClick={switchToMain}
          />
          <RadioOption
            caption="Dev"
            message="Tauri development build."
            checked
          />
        </OptionSet>
      </Show>

      <OptionSet name="Plugins Folder">
        <span
          class="block text-base text-neutral-200 px-3 py-1 hover:bg-neutral-400/20 rounded-md"
          onClick={changePluginsDir}>
          {app.plugins_dir() || './plugins'}
        </span>
      </OptionSet>

      <Show when={!window.isMac}>
        <OptionSet name="LoL Client Location" disabled={app.activation_mode() === ActivationMode.Universal}>
          <span
            class="block text-base text-neutral-200 px-3 py-1 hover:bg-neutral-400/20 rounded-md"
            onClick={changeLeagueDir}>
            {app.league_dir() || '(not selected)'}
          </span>
        </OptionSet>
      </Show>

      <Show when={window.isMac}>
        <LaunchSettings />
      </Show>

      <OptionSet name="Activation Mode">
        <Show when={!window.isMac}>
          <RadioOption
            caption="Universal"
            message="Apply to all League Clients, including live and PBE."
            checked={app.activation_mode() === ActivationMode.Universal}
            onClick={() => setActivationMode(ActivationMode.Universal)}
          />
          <RadioOption
            caption="Targeted"
            message="Apply to a specific League Client that you choose. Use it if you get access denied in Universal mode, except the Tencent server."
            checked={app.activation_mode() === ActivationMode.Targeted}
            onClick={() => setActivationMode(ActivationMode.Targeted)}
          />
        </Show>
        <Show when={window.isMac}>
          <RadioOption
            caption="On-demand"
            message="Apply to a specific League Client that you launch from the Riot Client. You have to keep Pengu running in background."
            disabled
            checked
          />
        </Show>
      </OptionSet>

    </div>
  )
}
