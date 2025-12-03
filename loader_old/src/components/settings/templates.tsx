import { children, Component, Show } from 'solid-js'
import { Checkbox, CheckboxProps } from '../ui/Checkbox'
import { RadioButton, RadioButtonProps } from '../ui/RadioButton'

export const OptionSet: Component<{
  name: string
  disabled?: boolean
  children: any
}> = (props) => {
  const c = children(() => props.children)
  return (
    <div class="aria-disabled:pointer-events-none aria-disabled:opacity-50" aria-disabled={props.disabled}>
      <h3 class="font-semibold text-neutral-400 text-sm mb-2">{props.name}</h3>
      <div class="space-y-4">
        {c()}
      </div>
    </div>
  )
}

export const CheckOption: Component<CheckboxProps & {
  caption: string
  message?: string
}> = (props) => {
  return (
    <label class="flex mb-4 space-x-4 aria-disabled:pointer-events-none aria-disabled:opacity-60" aria-disabled={props.disabled}>
      <div class="mt-1">
        <Checkbox {...props} />
      </div>
      <div class="flex flex-col justify-start">
        <h2 class="text-neutral-200">{props.caption}</h2>
        <Show when={props.message}>
          <p class="text-neutral-400 text-sm">{props.message}</p>
        </Show>
      </div>
    </label>
  )
}

export const RadioOption: Component<RadioButtonProps & {
  caption: string
  message: string
}> = (props) => {
  return (
    <label class="flex mb-4 space-x-4 aria-disabled:pointer-events-none aria-disabled:opacity-60" aria-disabled={props.disabled}>
      <div class="mt-1">
        <RadioButton {...props} />
      </div>
      <div class="flex flex-col justify-start">
        <h2 class="text-neutral-200">{props.caption}</h2>
        <p class="text-neutral-400 text-sm">{props.message}</p>
      </div>
    </label>
  )
}