import { JSXElement, Show, VoidComponent } from 'solid-js'

type NavigationItemProps = {
  caption: string
  icon?: JSXElement
  active?: boolean
  onClick?: (name: string) => void
}

export const NavigationItem: VoidComponent<NavigationItemProps> = (props) => {
  return (
    <li data-selected={props.active || 'false'} class="settings-navigation-item" onClick={() => props.onClick?.(props.caption)}>
      <a href='/products/league_of_legends/patchlines/live?settings=true&settings-page=league_of_legends'>
        {props.icon}
        <span class="text formatted-message settings-navigation-item-name" data-family="sans" data-bold="false" data-scale="LabelM" data-testid="settings-navigation-item-name">{props.caption}</span>
      </a>
    </li>
  )
}

export const SubNavigationItem: VoidComponent<NavigationItemProps> = (props) => {
  return (
    <li class="settings-navigation-item settings-sub-navigation-item" data-selected={props.active || 'false'} onClick={() => props.onClick?.(props.caption)}>
      <a>
        {props.icon}
        <span class="text formatted-message settings-navigation-item-name" data-family="sans" data-bold="false" data-scale="LabelS" data-testid="settings-sub-navigation-item-name">{props.caption}</span>
      </a>
    </li>
  )
}