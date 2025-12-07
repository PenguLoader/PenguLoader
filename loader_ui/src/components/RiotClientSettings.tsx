import { createEffect, createSignal, Match, onCleanup, onMount, Show, Switch, VoidComponent } from 'solid-js'
import { Dynamic, Portal } from 'solid-js/web'
import { watchElement } from '@lib/dom-utils'
import { SettingsContext } from './settings/context'
import NavigationItem from './settings/NavigationItem'
import * as Icons from './Icons'
import PenguIcon from '@assets/pengu-icon.png?inline'
import { SettingsDrawer } from './settings/SettingsDrawer'
import { PenguGeneralSettings, PenguLoLClientSettings, PenguRiotClientSettings } from './PenguSettings'

const RiotClientSettings: VoidComponent = () => {
  const [title, setTitle] = createSignal<string>('')
  const [pageId, setPageId] = createSignal<string>('/')
  const [pageComponent, setPageComponent] = createSignal<VoidComponent>()
  const [navList, setNavList] = createSignal<HTMLUListElement>()

  const restoreExtPage = () => {
    // const pageId = settings.lastRCPage()
    // if (pageId) {
    //   const peers = document.querySelectorAll('.settings-navigation-product-list>li')
    //   peers.forEach(el => el.setAttribute('data-selected', 'false'))
    // }
  }

  createEffect(async () => {
    const id = pageId()
    if (id.startsWith('/')) {
      setExtDrawerVisible(true)
    } else {
      clearExtActive()
      setExtDrawerVisible(false)
    }
  })

  const setExtDrawerVisible = (visible: boolean) => {
    const drawer = document.querySelector('.settings-drawer-details') as HTMLElement | null
    if (drawer) {
      drawer.style.display = visible ? 'block' : 'none'
    }
  }

  const clearExtActive = () => {
    const ul = navList()
    if (ul) {
      const lis = ul.querySelectorAll(':scope>li[data-selected="true"]')
      lis.forEach(li => li.setAttribute('data-selected', 'false'))
    }
  }

  const navClick = (e: PointerEvent) => {
    const ul = navList()!
    const target = e.target as HTMLElement

    ul.querySelectorAll('li').forEach((li) => {
      if (li.contains(target)) {
        const link = li.firstChild as HTMLAnchorElement
        const href = link?.getAttribute('href')
        if (href) {
          setPageId(href)
          li.setAttribute('data-selected', 'true')
        }
      }
    })
  }

  onMount(() => {
    const stop = watchElement<HTMLUListElement>('.settings-navigation-product-list', (el) => {
      if (el) {
        el.addEventListener('click', navClick)
      } else {
        navList()?.removeEventListener('click', navClick)
      }
      setNavList(el)
    })

    onCleanup(() => {
      stop()
      navList()?.removeEventListener('click', navClick)
    })
  })

  return (
    <SettingsContext.Provider value={{ pageId, setPageId, title, setTitle, pageComponent, setPageComponent }}>
      <Portal mount={navList()}>
        <NavigationItem
          name="Pengu Loader"
          icon={<img src={PenguIcon} class="settings-navigation-item-icon" />}
        >
          <NavigationItem.SubItem
            id="pengu-general"
            name="General"
            icon={<Icons.GeneralIcon />}
            page={PenguGeneralSettings}
          />
          <NavigationItem.SubItem
            id="pengu-riot-client"
            name="Riot Client"
            icon={<Icons.RiotIcon />}
            page={PenguRiotClientSettings}
          />
          <NavigationItem.SubItem
            id="pengu-lol-client"
            name="LoL Client"
            icon={<Icons.LeagueFlatIcon />}
            page={PenguLoLClientSettings}
          />
        </NavigationItem>
      </Portal>
      <Show when={!pageId().startsWith('/')}>
        <Portal mount={document.querySelector('#settings-drawer-content .settings-drawer-details')?.parentElement!}>
          <SettingsDrawer title={title()} icon={<img src={PenguIcon} class="settings-header-title-icon-image" />}>
            <Dynamic component={pageComponent()} />
          </SettingsDrawer>
        </Portal>
      </Show>
    </SettingsContext.Provider>
  )
}

export default RiotClientSettings