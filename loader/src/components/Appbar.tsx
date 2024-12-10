import {
  Component,
  createSignal,
  JSX,
  onMount,
  Show,
  splitProps,
} from 'solid-js'
import { appWindow } from '@tauri-apps/api/window'
import { twMerge } from 'tailwind-merge'
import { SettingsIcon, StoreIcon } from './Icons'
import { useRoot } from '../lib/root'
import { useTippy } from '../lib/utils'
import icon from '../assets/icon-sm.png'
import { useI18n } from '~/lib/i18n'

const Command: Component<JSX.HTMLAttributes<HTMLSpanElement>> = (props) => {
  const [local, rest] = splitProps(props, ['class'])
  return (
    <span
      class={twMerge(
        'flex justify-center items-center w-12 h-full hover:bg-foreground/15',
        local.class
      )}
      {...rest}
    />
  )
}

export const Appbar: Component<{
  isHome: boolean
}> = (props) => {
  const i18n = useI18n()
  const { settings, setStore } = useRoot()
  const [focus, setFocus] = createSignal(true)

  const minimize = () => appWindow.minimize()
  const close = () => {
    if (window.isMac) {
      appWindow.hide()
    } else {
      appWindow.close()
    }
  }

  onMount(async () => {
    setFocus(await appWindow.isFocused())
    appWindow.onFocusChanged((e) => setFocus(e.payload))
  })

  return (
    <div
      data-tauri-drag-region
      class='flex items-center justify-between h-10 aria-busy:bg-neutral-700 aria-busy:opacity-85'
      aria-busy={!focus()}
    >
      <div class='flex items-center px-[10px] h-full pointer-events-none'>
        <img src={icon} class='size-5 rounded-sm' />
        <span class='px-2 text-sm'>Pengu Loader</span>
        <span class='text-sm text-foreground/50'>v{window.appVersion}</span>
      </div>

      <div class='flex justify-center h-full text-foreground/80'>
        <Show when={props.isHome}>
          <Command
            onClick={() => setStore(true)}
            ref={useTippy(i18n.t('plugin_store'))}
          >
            <StoreIcon size={16} />
          </Command>
          <Command onClick={settings.show} ref={useTippy(i18n.t('settings'))}>
            <SettingsIcon size={16} />
          </Command>
        </Show>
        <Command onClick={minimize}>
          <svg width='10' height='10' viewBox='0 0 10.2 1' fill='currentColor'>
            <rect x='0' y='50%' width='10.2' height='1' />
          </svg>
        </Command>
        <Command onClick={close} class='hover:text-white hover:bg-red-600'>
          <svg width='10' height='10' viewBox='0 0 10 10' fill='currentColor'>
            <polygon points='10.2,0.7 9.5,0 5.1,4.4 0.7,0 0,0.7 4.4,5.1 0,9.5 0.7,10.2 5.1,5.8 9.5,10.2 10.2,9.5 5.8,5.1' />
          </svg>
        </Command>
      </div>
    </div>
  )
}
