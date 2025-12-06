import { createRoot, createSignal } from 'solid-js'

const root = createRoot(() => {

  return {
  }
})

const settings = createRoot(() => {
  const [pageId, setPageId] = createSignal<string>()
  const [lastRCPage, setLastRCPage] = createSignal<string>()
  
  return {
    pageId, setPageId,
    lastRCPage, setLastRCPage
  }
})

export const useRoot = () => root
export const useSettings = () => settings