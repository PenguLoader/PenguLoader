import { Show } from 'solid-js';
import { useRoot } from './root';

interface Props {
  item: Action
  index: number
  click: () => any
}

export function SearchItem(props: Props) {

  const { activeIndex, setActiveIndex } = useRoot();

  return (
    <div class="px-1">
      <div
        data-index={props.index}
        data-active={activeIndex() === props.index}
        onClick={props.click}
        onMouseMove={() => setActiveIndex(props.index)}
        class="cursor-pointer relative flex select-none items-center rounded px-2 py-1.5
          text-sm outline-none data-[disabled]:pointer-events-none data-[disabled]:opacity-50
          data-[active=true]:bg-slate-700 data-[active=true]:text-white"
      >
        <Show when={props.item.icon}>
          <span innerHTML={props.item.icon}></span>
        </Show>
        <span>{props.item.name}</span>
        <Show when={props.item.legend}>
          <span class="ml-auto text-xs tracking-widest">{props.item.legend}</span>
        </Show>
      </div>
    </div>
  )
}