import { createContext, Accessor, Setter, VoidComponent } from 'solid-js'

export const SettingsContext = createContext<{
  lastRCPage?: string
  title: Accessor<string>
  setTitle: Setter<string>
  pageId: Accessor<string>
  setPageId: Setter<string>
  pageComponent: Accessor<VoidComponent | undefined>
  setPageComponent: Setter<VoidComponent | undefined>
}>()