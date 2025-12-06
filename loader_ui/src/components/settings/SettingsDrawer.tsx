import { Component, JSXElement } from 'solid-js'

type SettingsDrawerProps = {
  title: string
  icon: JSXElement
  children?: JSXElement
}

export const SettingsDrawer: Component<SettingsDrawerProps> = (props) => {
  return (
    <div class="settings-drawer-details">
      <div class="settings-header-title">
        <div class="settings-header-title-icon">
          {props.icon}
        </div>
        <div class="settings-header-title-text">
          <span class="text formatted-message" data-family="riot-sans" data-bold="false" data-scale="HeadlineL" data-testid="text">{props.title}</span>
        </div>
      </div>
      <div class="general-settings">
        <div class="general-settings-list">
          <div class="settings-group">
            {props.children}
          </div>
        </div>
      </div>
    </div>
  )
}