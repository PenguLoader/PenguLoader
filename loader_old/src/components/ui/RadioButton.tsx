import { Component } from 'solid-js'

export type RadioButtonProps = {
  checked?: boolean
  disabled?: boolean
  onClick?: () => void
}

export const RadioButton: Component<RadioButtonProps> = (props) => {

  const click = () => {
    if (!props.disabled) {
      props.onClick?.()
    }
  }

  return (
    <button
      role="radio"
      type="button"
      class="shrink-0 size-4 flex cursor-default rounded-full border-2 border-primary/40 hover:border-primary aria-checked:border-primary aria-checked:bg-primary"
      onClick={click}
      aria-checked={props.checked}
      aria-disabled={props.disabled}
    >
      <span
        class="block mx-auto my-auto rounded-md size-3 border-2 border-black invisible aria-checked:visible"
        aria-checked={props.checked}
      >
      </span>
    </button>
  )
}