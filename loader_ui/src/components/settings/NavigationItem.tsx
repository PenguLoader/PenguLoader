import {
  JSXElement,
  Component, VoidComponent, Accessor, Setter,
  createContext, createSignal, useContext,
} from 'solid-js'
import { SettingsContext } from './context'

const NavContext = createContext<{
  rootName: string
  firstName: string
  items: string[]
  current: Accessor<string>
  setCurrent: Setter<string>
}>()

// function updatePageView(rootName: string, subName: string) {
//   const peers = document.querySelectorAll('.settings-navigation-product-list>li')
//   peers.forEach(el => el.setAttribute('data-selected', 'false'))

//   const caption = document.querySelector('.settings-header-title-text>span')!
//   caption.textContent = `${rootName} - ${subName}`
// }

const SubNavigationItem: VoidComponent<{
  id: string
  name: string
  icon?: JSXElement
}> = (props) => {

  const nav = useContext(NavContext)!
  const settings = useContext(SettingsContext)!

  nav.items.push(props.id)
  if (nav.items.length === 1) {
    nav.firstName = props.name
  }

  const activate = () => {
    nav.setCurrent(props.id)
    settings.setPageId(props.id)
    settings.setTitle(`${nav.rootName} - ${props.name}`)
  }

  return (
    <li
      class="settings-navigation-item settings-sub-navigation-item"
      data-selected={settings.pageId() === props.id}
      onClick={activate}
    >
      <a>
        {props.icon}
        <span class="text formatted-message settings-navigation-item-name"
          data-family="sans" data-bold="false" data-scale="LabelS">{props.name}</span>
      </a>
    </li>
  )
}

const NavigationItem: Component<{
  name: string
  icon?: JSXElement
  children?: JSXElement
}> = (props) => {

  const items = Array<string>()
  const settings = useContext(SettingsContext)!
  const [current, setCurrent] = createSignal<string>('')

  const ctx: typeof NavContext.defaultValue = {
    current, setCurrent,
    rootName: props.name,
    items,
    firstName: ''
  }

  const activate = async () => {
    const first = items[0]
    setCurrent(first)
    settings.setPageId(first)
    settings.setTitle(`${props.name} - ${ctx.firstName}`)
  }

  return (
    <NavContext.Provider value={ctx}>
      <li data-selected={items.includes(settings.pageId())} class="settings-navigation-item" onClick={activate}>
        <a>
          {props.icon}
          <span class="text formatted-message settings-navigation-item-name"
            data-family="sans" data-bold="false" data-scale="LabelM">{props.name}</span>
        </a>
      </li>
      {props.children}
    </NavContext.Provider>
  )
}

export default Object.assign(NavigationItem, {
  SubItem: SubNavigationItem
})