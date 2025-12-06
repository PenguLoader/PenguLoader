/* @refresh reload */

import 'uno.css'
import { render } from 'solid-js/web'
import 'solid-devtools'

import './global.css'
import App from './App'

let root = document.getElementById('pengu-root')

if (!root) {
  root = document.createElement('div')
  root.id = 'pengu-root'

  document.body.appendChild(root)
  render(() => <App />, root!)
}

// function watchSettingsNavigation(callback: (el: HTMLDivElement) => void) {
//   const selector = '.settings-navigation-product-list'
//   let last = document.querySelector(selector)
//   if (last) {
//     callback(last as HTMLDivElement)
//   }

//   const observer = new MutationObserver(() => {
//     let el = document.querySelector(selector)
//     if (el !== last) {
//       last = el
//       if (el) {
//         callback(el as HTMLDivElement)
//       }
//     }
//   })

//   observer.observe(document.documentElement, {
//     childList: true,
//     subtree: true
//   })
// }

// function documentReady() {
//   return new Promise<void>((resolve) => {
//     if (document.readyState === 'complete' || document.readyState === 'interactive') {
//       resolve()
//     } else {
//       window.addEventListener('DOMContentLoaded', () => resolve(), { once: true })
//     }
//   })
// }

// documentReady().then(() => {
//   watchSettingsNavigation((root) => {
//     root.style.display = 'flex'
//     root.style.flexDirection = 'column'
//     render(() => <ExtraProductSettings />, root)
//   })
// })