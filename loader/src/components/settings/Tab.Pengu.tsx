import { Component } from 'solid-js'
import { dialog } from '@tauri-apps/api'
import { Config, useConfig } from '~/lib/config'
import { LeagueClient } from '~/lib/league-client'
import { OptionSet, RadioOption } from './templates'
import { ActivationMode, CoreModule } from '~/lib/core-module'

export const TabPengu: Component = () => {

  const { app } = useConfig()

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

      <OptionSet name="Plugins Folder">
        <span
          class="block text-base text-neutral-200 px-3 py-1 hover:bg-neutral-400/20 rounded-md"
          onClick={changePluginsDir}>
          {app.plugins_dir() || './plugins'}
        </span>
      </OptionSet>

      <OptionSet name="LoL Client Location" disabled={app.activation_mode() === ActivationMode.Universal}>
        <span
          class="block text-base text-neutral-200 px-3 py-1 hover:bg-neutral-400/20 rounded-md"
          onClick={changeLeagueDir}>
          {app.league_dir() || '(not selected)'}
        </span>
      </OptionSet>

      <OptionSet name="Activation Mode">
        <RadioOption
          caption="Universal"
          message="Apply to all League Clients, including live and PBE."
          checked={app.activation_mode() === ActivationMode.Universal}
          onClick={() => setActivationMode(ActivationMode.Universal)}
        />
        <RadioOption
          caption="Targeted"
          message="Apply to a specific League Client to avoid UnauthorizedAccess issue on some Windows."
          checked={app.activation_mode() === ActivationMode.Targeted}
          onClick={() => setActivationMode(ActivationMode.Targeted)}
        />
        <RadioOption
          disabled
          caption="On-demand"
          message="Apply to a specific League Client that you launch, by tracking Riot Client. You have to keep Pengu running in background."
          checked={app.activation_mode() === ActivationMode.OnDemand}
          onClick={() => setActivationMode(ActivationMode.OnDemand)}
        />
      </OptionSet>

    </div>
  )
}