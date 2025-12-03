import { Component, For, Show, createMemo, createSignal } from 'solid-js'

const ComboboxItem: Component<{
  text: string
  selected: boolean
  onClick: () => void
}> = (props) => {
  return (
    <div
      onClick={props.onClick}
      class="relative flex cursor-pointer select-none items-center rounded-sm px-2 py-1.5 text-sm outline-none hover:bg-accent"
      role="option"
    >
      {props.text}
      <Show when={props.selected}>
        <svg width="15" height="15" viewBox="0 0 15 15" fill="none" class="ml-auto h-4 w-4">
          <path d="M11.4669 3.72684C11.7558 3.91574 11.8369 4.30308 11.648 4.59198L7.39799 11.092C7.29783 11.2452 7.13556 11.3467 6.95402 11.3699C6.77247 11.3931 6.58989 11.3355 6.45446 11.2124L3.70446 8.71241C3.44905 8.48022 3.43023 8.08494 3.66242 7.82953C3.89461 7.57412 4.28989 7.55529 4.5453 7.78749L6.75292 9.79441L10.6018 3.90792C10.7907 3.61902 11.178 3.53795 11.4669 3.72684Z" fill="currentColor" fill-rule="evenodd" clip-rule="evenodd"></path>
        </svg>
      </Show>
    </div>
  )
}

interface Item {
  id: string
  name: string
  selected?: boolean
}

interface Props {
  items: Array<Item>
  selected?: string | number
  onSelect?: (id: string, index: number) => void
}

export const ComboBox: Component<Props> = (props) => {

  const [opened, setOpened] = createSignal(false)
  const selected = createMemo(() => {
    if (typeof props.selected === 'number') {
      return Math.min(props.selected, props.items.length)
    } else if (typeof props.selected === 'string') {
      for (let i = 0; i < props.items.length; i++) {
        if (props.items[i].id === props.selected) {
          return i
        }
      }
    }
    return 0
  })

  const select = (id: string, index: number) => {
    setOpened(false)
    props.onSelect?.(id, index)
  }

  return (
    <div class="relative min-w-48">
      <button
        onClick={() => setOpened(v => !v)}
        class="relative inline-flex items-center whitespace-nowrap rounded-md text-sm font-medium 
          transition-colors focus-visible:outline-none focus-visible:ring-1 focus-visible:ring-ring 
          disabled:pointer-events-none disabled:opacity-50 border border-input bg-background shadow-sm 
          hover:bg-accent hover:text-accent-foreground h-9 px-4 py-2 w-full justify-between"
        role="combobox"
        aria-expanded={opened()}
        type="button"
      >
        <span>
          {props.items[selected()]?.name ?? ''}
        </span>
        <svg
          width="15"
          height="15"
          viewBox="0 0 15 15"
          fill="none"

          class="ml-2 h-4 w-4 shrink-0 opacity-50"
        >
          <path
            d="M4.93179 5.43179C4.75605 5.60753 4.75605 5.89245 4.93179 6.06819C5.10753 6.24392 5.39245 6.24392 5.56819 6.06819L7.49999 4.13638L9.43179 6.06819C9.60753 6.24392 9.89245 6.24392 10.0682 6.06819C10.2439 5.89245 10.2439 5.60753 10.0682 5.43179L7.81819 3.18179C7.73379 3.0974 7.61933 3.04999 7.49999 3.04999C7.38064 3.04999 7.26618 3.0974 7.18179 3.18179L4.93179 5.43179ZM10.0682 9.56819C10.2439 9.39245 10.2439 9.10753 10.0682 8.93179C9.89245 8.75606 9.60753 8.75606 9.43179 8.93179L7.49999 10.8636L5.56819 8.93179C5.39245 8.75606 5.10753 8.75606 4.93179 8.93179C4.75605 9.10753 4.75605 9.39245 4.93179 9.56819L7.18179 11.8182C7.35753 11.9939 7.64245 11.9939 7.81819 11.8182L10.0682 9.56819Z"
            fill="currentColor"
            fill-rule="evenodd"
            clip-rule="evenodd"
          ></path>
        </svg>
      </button>
      <Show when={opened()}>
        <div class="absolute top-full left-0 z-50 mt-1 w-full">
          <div
            role="dialog"
            class="rounded-md border bg-popover text-popover-foreground shadow-md outline-none"
            tabindex="-1"
          >
            <div class="flex h-full w-full flex-col overflow-hidden rounded-md bg-popover text-popover-foreground">
              <div class="overflow-hidden p-1 text-foreground" role="presentation">
                <div role="group">
                  <For each={props.items}>
                    {(item, index) => <ComboboxItem
                      text={item.name}
                      selected={selected() === index()}
                      onClick={() => select(item.id, index())}
                    />}
                  </For>
                </div>
              </div>
            </div>
          </div>
        </div>
      </Show>
    </div>
  )
}