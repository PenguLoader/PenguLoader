import { createSignal, onCleanup, onMount } from 'solid-js'
import tippy, { type Instance as TippyInstance } from 'tippy.js'

export function isMac() {
  return /mac/i.test(navigator.platform)
}

// fnv1a 32-bit
export function getHash(str: string) {
  const data = new TextEncoder().encode(str)
  let hash = 0x811c9dc5

  for (const byte of data) {
    hash ^= byte
    hash += (hash << 1) + (hash << 4) + (hash << 7) + (hash << 8) + (hash << 24)
  }

  return hash >>> 0
}

export function useTippy(tooltip: string) {
  let instance: TippyInstance
  const [ref, setRef] = createSignal<HTMLElement>(null!)

  onMount(() => {
    instance = tippy(ref(), {
      content: tooltip,
      arrow: false,
    })
  })

  onCleanup(() => {
    instance?.destroy()
  })

  return setRef
}