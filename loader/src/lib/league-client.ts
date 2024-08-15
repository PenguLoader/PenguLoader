import { exists, readTextFile } from '@tauri-apps/api/fs'
import { join } from '@tauri-apps/api/path'

interface RiotClientInstalls {
  associated_client: Record<string, string>
  rc_default: string
  rc_live: string
}

export const LeagueClient = new class {

  /**
   * Validate League folder, it must contain LCUX exe.
   */
  async validateDir(dir: string): Promise<boolean> {
    if (dir && typeof dir === 'string') {
      const uxPath = await join(dir, 'LeagueClientUx.exe')
      return await exists(uxPath)
    }
    return false
  }

  async correctDir() {

  }

  /**
   * Find League from the RiotClient manifest.
   */
  async findLeaguePath(): Promise<string | null> {
    const jsonPath = 'C:\\ProgramData\\Riot Games\\RiotClientInstalls.json'

    try {
      if (await exists(jsonPath)) {
        const json = await readTextFile(jsonPath)
        const data = <RiotClientInstalls>JSON.parse(json)

        let rcPath = ''
        if (data.rc_live)
          rcPath = await join(data.rc_live, '..')
        else if (!data.rc_default)
          rcPath = await join(data.rc_default, '..')

        let lcDir = await join(rcPath, '..', 'League of Legends')
        if (await this.validateDir(lcDir))    // found
          return await join(lcDir)

        let lcPbeDir = await join(rcPath, '..', 'League of Legends (PBE)')
        if (await this.validateDir(lcPbeDir)) // found PBE
          return await join(lcPbeDir)

        if (typeof data.associated_client.Count === 'object') {
          for (let k in data.associated_client) {
            if (/\(pbe\)/i.test(k))
              lcPbeDir = k.replace(/[\\\/]$/, '')
            else
              lcDir = k.replace(/[\\\/]$/, '')
          }

          if (await this.validateDir(lcDir))    // found
            return lcDir
          else if (await this.validateDir(lcPbeDir)) // found PBE
            return lcPbeDir
        }
      }
    }
    catch {
    }

    return null
  }

}