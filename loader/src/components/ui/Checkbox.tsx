import type { Component } from 'solid-js'

export type CheckboxProps = {
  checked?: boolean
  disabled?: boolean
  onChange?: (checked: boolean) => void
  onClick?: () => void
}

export const Checkbox: Component<CheckboxProps> = (props) => {

  const click = () => {
    if (!props.disabled) {
      props.onClick?.()
      props.onChange?.(!(props.checked === true))
    }
  }

  return (
    <button
      class="size-4 shrink-0 border-2 border-primary/40 flex items-center justify-center rounded-sm cursor-default
      hover:border-primary aria-checked:bg-primary aria-checked:border-primary
      disabled:bg-muted disabled:opacity-50 disabled:hover:border-primary/40"
      disabled={props.disabled}
      aria-checked={props.checked}
      onClick={click}
    >
      <span
        class="text-accent invisible aria-checked:visible"
        aria-checked={props.checked}
      >
        <svg width="10" height="8" fill="currentColor" xmlns="http://www.w3.org/2000/svg">
          <path fill-rule="evenodd" clip-rule="evenodd" d="M9.143 1.433a.5.5 0 0 1 .007.707L4.04 7.346a.5.5 0 0 1-.74-.029L.82 4.355a.5.5 0 0 1 .063-.704l.639-.534a.5.5 0 0 1 .704.062l1.536 1.835L7.842.857A.5.5 0 0 1 8.549.85l.594.583Z" />
        </svg>
      </span>
    </button>
  )
}