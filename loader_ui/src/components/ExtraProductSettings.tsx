import { onMount } from 'solid-js'
import { NavigationItem, SubNavigationItem } from './NavigationItem'
import { GeneralIcon, LeagueFlatIcon } from './Icons'
import PenguIcon from '../assets/pengu-icon.png?inline'
import { SettingsDrawer } from './settings/SettingsDrawer'


export function ExtraProductSettings() {
  const section = 'Pengu Loader'

  const navigateTo = (name: string) => {
    const peers = document.querySelectorAll('.settings-navigation-product-list>li')
    peers.forEach(el => el.setAttribute('data-selected', 'false'))

    const caption = document.querySelector('.settings-header-title-text>span')!
    caption.textContent = `${section} - ${name}`
  }

  onMount(() => {
    if (!document.getElementById('pengu-settings-drawer')) {
      const contentDiv = document.querySelector('[data-testid=settings-drawer-main] .ScrollbarsCustom-Content')!
      const drawerDiv = document.createElement('div')
      drawerDiv.id = 'pengu-settings-drawer'
      drawerDiv.classList.add('settings-drawer-details')
      contentDiv.appendChild(drawerDiv)
    }
  })

  return (
    <div class="flex flex-col order-last mt-8">
      <NavigationItem caption={section} icon={<img src={PenguIcon} class="settings-navigation-item-icon" />} />
      <SubNavigationItem caption="General" icon={<GeneralIcon />} onClick={navigateTo} />
      <SubNavigationItem caption="LoL Client" icon={<LeagueFlatIcon />} onClick={navigateTo} />

      <SettingsDrawer title={section} icon={<img src={PenguIcon} class="settings-header-title-icon-image" />} />
    </div>
  )
}