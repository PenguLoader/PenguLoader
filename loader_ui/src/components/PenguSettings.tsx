import { Component } from 'solid-js'
import { DirectorySetting } from "./settings/DirectorySetting"
import { RadioGroupSetting, RadioSetting } from "./settings/RadioGroupSetting"
import { SectionSettings } from "./settings/SectionSetting"
import { ToggleSetting } from './settings/ToggleSetting'


export const PenguGeneralSettings: Component = () => {

  return (
    <>
      <div>
        <SectionSettings name="Plugins folder" />
        <DirectorySetting path="C:\Program Files\Pengu Loader\plugins" />
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

export const PenguLoLClientSettings: Component = () => {
  return (
    <>
      <div>
        <SectionSettings name="Client tweaks" />
        <ToggleSetting
          caption="Enable hot keys" checked recommended
          description="Allows Pengu to capture the hotkeys below to perform corresponding actions."
        />

        <div>
          <span class="text formatted-message" data-family="sans" data-bold="false" data-scale="LabelXS" data-testid="text">
            <span class="formatted-message case-upper">Hot keys</span>
          </span>
          <div class="flex items-center mt-2 gap-2">
            <div class="chip-container" data-inline-chip="false">
              <span class="text formatted-message chip-title" data-family="riot-sans" data-bold="false" data-scale="LabelXS" data-testid="text">
                <span class="formatted-message">CTRL SHIFT R</span>
              </span>
            </div>
            <span class="text formatted-message" data-family="sans" data-bold="false" data-scale="BodyS" data-testid="text">
              <span class="formatted-message">— Reload the client interface</span>
            </span>
          </div>
          <div class="flex items-center mt-2 gap-2">
            <div class="chip-container" data-inline-chip="false">
              <span class="text formatted-message chip-title" data-family="riot-sans" data-bold="false" data-scale="LabelXS" data-testid="text">
                <span class="formatted-message">CTRL SHIFT ENTER</span>
              </span>
            </div>
            <span class="text formatted-message" data-family="sans" data-bold="false" data-scale="BodyS" data-testid="text">
              <span class="formatted-message">— Restart the UX process</span>
            </span>
          </div>
          <div class="flex items-center mt-2 gap-2">
            <div class="chip-container" data-inline-chip="false">
              <span class="text formatted-message chip-title" data-family="riot-sans" data-bold="false" data-scale="LabelXS" data-testid="text">
                <span class="formatted-message">CTRL SHIFT I</span>
              </span>
            </div>
            <span class="text formatted-message" data-family="sans" data-bold="false" data-scale="BodyS" data-testid="text">
              <span class="formatted-message">— Open developer tools</span>
            </span>
          </div>
        </div>

        <ToggleSetting
          caption="Optimized client" checked recommended
          description="Enables caching and disables unnecessary background features. This option does not affect your connection."
        />
        <ToggleSetting
          caption="Super potato mode" checked recommended
          description="Disables animations and transitions to reduce input lag."
        />
        <ToggleSetting
          caption="Silent mode"
          description="Suppresses notifications and prevents the client window from flashing when a match is found."
        />
      </div>
      <div>
        <SectionSettings name="Developer options" />
        <ToggleSetting
          caption="Enable debug logging" checked recommended
          description="Logs detailed information for troubleshooting."
        />
        <ToggleSetting
          caption="Developer tools"
          description="Allows you to open Chrome DevTools to debug the UX and plugins."
        />
        <ToggleSetting
          caption="Insecure mode"
          description="Disables all web security features, such as CORS and CSP."
        />
        <ToggleSetting
          caption="RiotClient API"
          description="Allows access to the RiotClient API via the 'riotclient' domain."
        />
        <ToggleSetting
          caption="Enable proxy"
          description="Routes UX request traffic through a network proxy."
        />
      </div>
    </>
  )
}