import { Component } from 'solid-js'
import { useConfig } from '~/lib/config'
import { CheckOption, OptionSet } from './templates'

export const TabClient: Component = () => {

  const { client } = useConfig()

  return (
    <div class="space-y-4">

      <p class="text-sm text-neutral-400">*These options work within the Client, restart it to take effect.</p>

      <OptionSet name="Hot Keys">
        <CheckOption
          caption="Enable hot keys"
          message="Allow Pengu to catch these hot keys when you press in the Client to perform functions below."
          checked={client.use_hotkeys()}
          onChange={client.use_hotkeys}
        />
        <div class="space-y-2 ml-8 aria-disabled:opacity-50" aria-disabled={!client.use_hotkeys()}>
          <div class="flex items-center space-x-2">
            <kbd class="px-2 py-0.5 rounded-sm text-xs bg-neutral-500/30">Ctrl Shift R</kbd>
            <p class="text-sm text-neutral-400">Reload the Client</p>
          </div>
          <div class="flex items-center space-x-2">
            <kbd class="px-2 py-0.5 rounded-sm text-xs bg-neutral-500/30">Ctrl Shift Enter</kbd>
            <p class="text-sm text-neutral-400">Restart the UX</p>
          </div>
          <div class="flex items-center space-x-2 aria-disabled:line-through" aria-disabled={!client.use_devtools()}>
            <kbd class="px-2 py-0.5 rounded-sm text-xs bg-neutral-500/30">Ctrl Shift I</kbd>
            <span>/</span>
            <kbd class="px-2 py-0.5 rounded-sm text-xs bg-neutral-500/30">F12</kbd>
            <p class="text-sm text-neutral-400">Open Chrome DevTools</p>
          </div>
        </div>
      </OptionSet>

      <OptionSet name="Tweaks">
        <CheckOption
          caption="Optimized Client"
          message="Enable caching and disable some unnecessary things under the Client. This option does not cause your connection issues."
          checked={client.optimized_client()}
          onChange={client.optimized_client}
        />
        <CheckOption
          caption="Super Potato Mode"
          message="Disable all animations and transitions, also reduce input lag from the Client."
          checked={client.super_potato()}
          onChange={client.super_potato}
        />
        <CheckOption
          caption="Silent Mode"
          message="Suppress all notifications and flashing foreground window when matchmaking found."
          checked={client.silent_mode()}
          onChange={client.silent_mode}
        />
        <CheckOption
          caption="Disable Logging"
          message="Prevent the Client from collecting and storing log files, which may include sensitive information."
          checked={client.no_logging()}
          onChange={client.no_logging}
        />
      </OptionSet>

      <OptionSet name="Developer">
        <CheckOption
          caption="Developer Tools"
          message="Allow you to open Chrome DevTools to debug the UX and plugins."
          checked={client.use_devtools()}
          onChange={client.use_devtools}
        />
        <CheckOption
          caption="Insecure Mode"
          message="Disable all web security features like CORS and CSP."
          checked={client.insecure_mode()}
          onChange={client.insecure_mode}
        />
        <CheckOption
          caption="RiotClient API"
          message="Allow you to access RiotClient API via 'riotclient' domain."
          checked={client.use_riotclient()}
          onChange={client.use_riotclient}
        />
        <CheckOption
          caption="Allow Proxy"
          message="Allow the UX requests traffic through network proxy."
          checked={client.use_proxy()}
          onChange={client.use_proxy}
        />
      </OptionSet>

    </div>
  )
}