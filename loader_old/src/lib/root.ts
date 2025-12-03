import { createRoot, createSignal } from 'solid-js'

function useSettings() {
  const [visible, setVisible] = createSignal(false)
  const show = () => setVisible(true)
  const hide = () => setVisible(false)

  return {
    visible,
    show, hide,
  }
}

const _root = createRoot(() => {
  const [ready, setReady] = createSignal(false)
  const [isStore, setStore] = createSignal(false)
  const settings = useSettings()

  return {
    ready, setReady,
    isStore, setStore,
    settings,
  }
})

export const useRoot = () => _root