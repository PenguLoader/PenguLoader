import { ipcSend, riotPOST } from './ipc'

export function openDevTools() {
  ipcSend('open-devtools')
}

export function reloadClient() {
  // ipcSend('reload-client')
  window.location.reload()
}

export function restartClient() {
  riotPOST('/riot-client-lifecycle/v1/restart')
}