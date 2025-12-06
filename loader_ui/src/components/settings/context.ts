import { createContext, Accessor, Setter } from 'solid-js'

export const SettingsContext = createContext<{
  lastRCPage?: string
  title: Accessor<string>
  setTitle: Setter<string>
  pageId: Accessor<string>
  setPageId: Setter<string>
}>()