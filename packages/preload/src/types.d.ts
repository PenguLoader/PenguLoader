// Re-exports from `@pengujs/types` (the public plugin-author types package).
// `index.d.ts` already augments `Window`; the `import('@pengujs/types').*` aliases
// below promote the named exports to ambient globals so internal call sites can
// keep referencing `Action` / `Schema` / `Field` without per-file imports.

import type * as Pengu from '@pengujs/types'

declare global {
  type RcpApi        = Pengu.RcpApi
  type PluginGraph   = Pengu.PluginGraph
  type SocketApi     = Pengu.SocketApi
  type SocketEventData = Pengu.SocketEventData
  type Action        = Pengu.Action
  type Toast         = Pengu.Toast
  type ToastOptions  = Pengu.ToastOptions
  type ToastPosition = Pengu.ToastPosition
  type ToastType     = Pengu.ToastType
  type DataStore     = Pengu.DataStore
  type Effect        = Pengu.Effect
  type Settings      = Pengu.Settings
  type Schema        = Pengu.Schema
  type Field         = Pengu.Field

  interface RcpAnnouceEvent extends CustomEvent {
    errorHandler: () => any
    registrationHandler: (registrar: (e: any) => Promise<any>) => Promise<any> | void
  }
}

// `PluginModule` (the loader's import type) lives in `@pengujs/types` —
// import it explicitly where needed instead of polluting the global namespace
// next to the deprecated DOM `Plugin` interface.