import { invoke } from '@tauri-apps/api'
import { Config } from './config'
import { LeagueClient } from './league-client'

export enum ActivationMode {
  Universal = 0,
  Targeted,
  OnDemand,
}

export const CoreModule = new class {

  private useSymlink(): boolean {
    return Config.get('app', 'activation_mode')
      === ActivationMode.Targeted
  }

  /**
   * Check League dir is required or not.
   * Symlink mode must have it to activate.
   */
  async checkLeagueDir(): Promise<boolean> {
    if (this.useSymlink()) {
      const leagueDir = Config.get('app', 'league_dir')
      if (!await LeagueClient.validateDir(leagueDir)) {
        return false
      }
    }
    return true
  }

  /**
   * Check if the core module exists or not.
   */
  async exists(): Promise<boolean> {
    return await invoke<boolean>('plugin:config|core_exists')
  }

  /**
   * Check if the core module is activated or not.
   */
  async isActivated(): Promise<boolean> {
    if (window.isMac) {
      return await invoke<boolean>('plugin:macos|cmd_is_active')
    }
    return await invoke<boolean>('plugin:windows|core_is_activated', {
      symlink: this.useSymlink()
    })
  }

  /**
   * Perform activation.
   * @returns A boolean that indicates the new active state,
   *    the action is successful when error message is empty.
   */
  async doActivate(active: boolean): Promise<{ error: string, activated: boolean }> {
    let error = ''

    if (window.isMac) {
      await invoke('plugin:macos|cmd_set_active', {
        active: active,
      })
    } else {
      error = await invoke<string>('plugin:windows|core_do_activate', {
        active: active,
        symlink: this.useSymlink(),
      })
    }

    return {
      error: error,
      activated: await this.isActivated(),
    }
  }
}