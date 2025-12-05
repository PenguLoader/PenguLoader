import { children, Component, JSXElement, mergeProps, Show, VoidComponent } from "solid-js"

type RadioSettingProps = {
  caption: string
  description?: string
  recommended?: boolean
  checked?: boolean
  disabled?: boolean
}

export const RadioSetting: VoidComponent<RadioSettingProps> = (props) => {
  const merged = mergeProps({ checked: false, disabled: false }, props)
  return (
    <label class="radio-button">
      <div class="radio-button-wrapper">
        <input
          data-testid="radio-button-input"
          name="closeWindow"
          class="radio-button-input"
          type="radio"
          value="true"
          checked={merged.checked}
          disabled={merged.disabled}
        />
        <div class="radio-button-highlight"></div>
        <div class="radio-button-title" data-testid="radio-button-title">
          <span class="text formatted-message" data-family="sans" data-bold="false" data-scale="LabelM" data-testid="text">
            <span class="formatted-message">{props.caption}</span>
            <Show when={props.recommended}>
              <span class="general-title-label">
                <div class="chip-container" data-inline-chip="false" data-testid="chip-container">
                  <span class="text formatted-message chip-title" data-family="sans" data-bold="false"
                    data-scale="LabelXS" data-testid="text">RECOMMENDED</span>
                </div>
              </span>
            </Show>
          </span>
        </div>
      </div>
      <Show when={props.description}>
        <div class="radio-button-description" data-testid="radio-button-description">
          <span class="text formatted-message" data-family="sans" data-bold="false" data-scale="BodyS"
            data-testid="text">{props.description}</span>
        </div>
      </Show>
    </label>
  )
}

type RadioGroupSettingProps = {
  children?: JSXElement
}

export const RadioGroupSetting: Component<RadioGroupSettingProps> = (props) => {
  const c = children(() => props.children)
  return (
    <div class="radio-group" data-testid="radio-group">
      {c()}
    </div>
  )
}