import { mergeProps, Show, VoidComponent } from 'solid-js'

type ToggleSwitchProps = {
  checked?: boolean
  disabled?: boolean
}

const ToggleSwitch: VoidComponent<ToggleSwitchProps> = (props) => {
  const id = 'settings-toggle-block-toggle'
  const merged = mergeProps({ checked: false, disabled: false }, props)
  return (
    <label
      class="toggle"
      for={id}
      data-checked={merged.checked}
      data-disabled={merged.disabled}
    >
      <input
        type="checkbox"
        class="toggle-input"
        id={id} checked={merged.checked}
        disabled={merged.disabled}
      />
      <div class="toggle-slide-background" />
      <div class="toggle-slide">
        <svg width="16" height="16" viewBox="0 0 16 16" fill="currentColor" xmlns="http://www.w3.org/2000/svg" role="img" class="icon display-block toggle-icon" data-testid="icon" data-icon="checkmark">
          <path d="M13.136 2.86h.004l1.333 1.334v.003a.665.665 0 010 .94v.003l-8 8H6.47a.665.665 0 01-.939 0h-.003l-4-4v-.004a.665.665 0 010-.938v-.004L2.86 6.86h.004a.665.665 0 01.939 0h.004L6 9.054l6.193-6.194h.004a.665.665 0 01.94 0z" />
        </svg>
      </div>
    </label>
  )
}

type ToggleSettingProps = {
  caption: string
  description: string
  recommended?: boolean
  checked?: boolean
  disabled?: boolean
}

export const ToggleSetting: VoidComponent<ToggleSettingProps> = (props) => {
  return (
    <div class="settings-toggle-block-content-wrapper justify-between">
      <div class="settings-toggle-block-text-wrapper">
        <span class="text formatted-message settings-toggle-block-label" data-family="sans" data-bold="false" data-scale="LabelM" data-testid="text">
          <span class="formatted-message">{props.caption}</span>
          <Show when={props.recommended}>
            <div class="chip-container" data-inline-chip="false" data-testid="chip-container">
              <span class="text formatted-message chip-title" data-family="sans" data-bold="false" data-scale="LabelXS" data-testid="text">
                <span class="formatted-message">RECOMMENDED</span>
              </span>
            </div>
          </Show>
        </span>
        <span class="text formatted-message settings-toggle-block-description" data-family="sans" data-bold="false" data-scale="BodyS" data-testid="text">
          <span class="formatted-message">{props.description}</span>
        </span>
      </div>
      <div>
        <ToggleSwitch checked={props.checked} disabled={props.disabled} />
      </div>
    </div>
  )
}