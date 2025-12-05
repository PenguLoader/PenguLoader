import { DirectorySetting } from "./settings/DirectorySetting"
import { RadioGroupSetting, RadioSetting } from "./settings/RadioGroupSetting"
import { SectionSettings } from "./settings/SectionSetting"


export const PenguGeneralSettings = () => {

  return (
    <>
      <div>
        <SectionSettings name="Plugins folder" />
        <DirectorySetting path="C:\\Program Files\\Pengu Loader\\plugins" />
      </div>
      <div>
        <SectionSettings name="Activation mode" />
        <RadioGroupSetting>
          <RadioSetting caption="Global" description="Apply to all League Clients, including LIVE and PBE." recommended />
          <RadioSetting caption="On-demand" description="Apply only when a League Client is launched." />
        </RadioGroupSetting>
      </div>
    </>
  )
}

export const PenguLoLClientSettings = () => {
  return (
    <>
      <div>
        <SectionSettings name="League Client path" />
        <DirectorySetting path="C:\\Riot Games\\League of Legends\\LeagueClient.exe" />
      </div>
      <div>
        <SectionSettings name="Developer options" />
        <RadioGroupSetting>
          <RadioSetting caption="Launch with Pengu Loader" description="Start the League Client with Pengu Loader enabled." recommended />
          <RadioSetting caption="Launch normally" description="Start the League Client without Pengu Loader." />
        </RadioGroupSetting>
      </div>
    </>
  )
}