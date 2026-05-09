import { Component, createSignal, JSX, onMount, Show, splitProps } from 'solid-js'
import { twMerge } from 'tailwind-merge'
import { ArrowBackUpIcon, SettingsIcon, StoreIcon } from './Icons'
import { useRoot } from '../lib/root'
import { useTippy } from '../lib/utils'
import { pengu } from '../lib/pengu'
import { Activator } from './Activator'
import icon from '../assets/icon-sm.png'

const Command: Component<JSX.HTMLAttributes<HTMLButtonElement>> = (props) => {
  const [local, rest] = splitProps(props, ['class'])
  return (
    <button
      class={twMerge("flex justify-center items-center w-12 h-full hover:bg-foreground/15", local.class)}
      {...rest}
    />
  )
}

export const Appbar: Component<{
  isHome: boolean
}> = (props) => {

  const { settings, isStore, setStore } = useRoot()
  // Focus state via standard DOM events. WebView2 fills the entire window
  // client area, so window focus and DOM focus are equivalent — no host
  // bridge call needed.
  const [focus, setFocus] = createSignal(document.hasFocus())

  const minimize = () => pengu.host.minimize()
  // Mode-conditional close behavior is handled host-side in BorderlessWindow's
  // WndProc (Universal -> exit, OnDemand -> hide-to-tray). Hub just posts close.
  const close = () => pengu.host.close()

  onMount(() => {
    const onFocus = () => setFocus(true)
    const onBlur = () => setFocus(false)
    window.addEventListener('focus', onFocus)
    window.addEventListener('blur', onBlur)
  })

  return (
    <div
      class="app-drag flex items-center justify-between h-10 aria-busy:bg-neutral-700 aria-busy:opacity-85"
      aria-busy={!focus()}
    >

      <div class="flex items-center px-[10px] h-full pointer-events-none">
        <img src={icon} class="size-5 rounded-sm" />
        <span class="px-2 text-sm">Pengu Loader</span>
        <span class="text-sm text-foreground/50">v{window.appVersion}</span>
      </div>

      <div class="flex justify-center h-full text-foreground/80">
        <Show when={props.isHome}>
          {/* Activator lives in the appbar across both gallery and store views.
              Activation state is independent of which page the user is on. */}
          <Activator />
          <Show when={!isStore()}>
            <Command onClick={() => setStore(true)} ref={useTippy('Plugin Store')}>
              <StoreIcon size={16} />
            </Command>
          </Show>
          <Show when={isStore()}>
            <Command onClick={() => setStore(false)} ref={useTippy('Back to Gallery')}>
              <ArrowBackUpIcon size={16} />
            </Command>
          </Show>
          <Command onClick={settings.show} ref={useTippy('Settings')}>
            <SettingsIcon size={16} />
          </Command>
        </Show>
        <Command onClick={minimize}>
          <svg width="10" height="10" viewBox="0 0 10.2 1" fill="currentColor">
            <rect x="0" y="50%" width="10.2" height="1" />
          </svg>
        </Command>
        <Command onClick={close} class="hover:text-white hover:bg-red-600">
          <svg width="10" height="10" viewBox="0 0 10 10" fill="currentColor">
            <polygon points="10.2,0.7 9.5,0 5.1,4.4 0.7,0 0,0.7 4.4,5.1 0,9.5 0.7,10.2 5.1,5.8 9.5,10.2 10.2,9.5 5.8,5.1" />
          </svg>
        </Command>
      </div>

    </div>
  )
}
