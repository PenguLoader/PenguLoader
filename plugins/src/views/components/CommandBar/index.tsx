import { For, Show, createEffect, createMemo, on, onMount } from 'solid-js';
import Fuse from 'fuse.js';
import { useRoot } from './root';
import { SearchBar } from './SearchBar';
import { SearchItem } from './SearchItem';
import { DEFAULT_ACTIONS } from './data';

export function CommandBar() {

  let itemsList: HTMLDivElement;
  const { search, activeIndex, setActiveIndex, visible, setVisible } = useRoot();

  const filteredItems = createMemo(() => {
    if (search().length === 0) {
      return DEFAULT_ACTIONS
        .filter(item => !item.hidden);
    }

    const fuse = new Fuse(DEFAULT_ACTIONS, {
      includeScore: true,
      keys: ['name', 'tags', 'group']
    });

    return fuse.search(search())
      .filter(item => !item.item.hidden || (item.item.hidden && item.score! > 0))
      .sort((a, b) => b.score! - a.score!)
      .map(item => item.item);
  });

  createEffect(on(filteredItems, () => {
    setActiveIndex(0);
    scrollToActiveItem();
  }));

  function shouldShowCategory(item, index) {
    if (index == 0) return true;
    if (typeof filteredItems()[index - 1] === 'undefined') return false;
    return item.group !== filteredItems()[index - 1].group;
  }

  function scrollToActiveItem() {
    const activeElement: HTMLElement = itemsList.querySelector(`div[data-index="${activeIndex()}"]`)!;
    if (!activeElement) return;
    const newScrollPos = (activeElement.offsetTop + activeElement.offsetHeight) - itemsList.offsetHeight;
    if (newScrollPos > 0) {
      itemsList.scrollTop = newScrollPos;
    } else {
      itemsList.scrollTop = 0;
    }
  }

  function execute(index: number) {
    const item = filteredItems()[index];
    setActiveIndex(index);
    setVisible(false);
    if (typeof item.perform === 'function') {
      item.perform(item.id);
    }
  }

  function handleKeydown(e: KeyboardEvent) {
    if (e.isComposing) return;
    if (e.key === 'ArrowUp') {
      setActiveIndex(index => index && index - 1);
      scrollToActiveItem();
    } else if (e.key === 'ArrowDown') {
      setActiveIndex(index => index < filteredItems().length - 1 ? index + 1 : index);
      scrollToActiveItem();
    } else if (e.key === 'Enter') {
      execute(activeIndex());
    }
  }

  onMount(() => {
    window.addEventListener('keydown', e => {
      if (e.ctrlKey && e.code === 'KeyK') {
        e.preventDefault();
        setVisible(true);
      } else if (e.code === 'Escape') {
        if (visible()) {
          setVisible(false);
        }
      }
    });
  });

  return (
    <Show when={visible()}>
      <div class="fixed top-0 left-0 z-[99] flex items-center justify-center w-screen h-screen">
        <div
          onClick={() => setVisible(false)}
          class="absolute inset-0 w-full h-full bg-black bg-opacity-40"
        />
        <div class="flex min-h-[370px] justify-center w-full max-w-xl items-start relative">
          <div
            class="box-border flex flex-col w-full h-full overflow-hidden bg-white rounded-md shadow-md bg-opacity-90 drop-shadow-md backdrop-blur-sm"
            onKeyDown={handleKeydown}
          >
            <SearchBar />
            <div ref={itemsList!} class="max-h-[320px] overflow-y-auto overflow-x-hidden scroll-smooth">
              <For each={filteredItems()}>
                {(item, index) => (
                  <div class="pb-1 space-y-1">
                    <Show when={shouldShowCategory(item, index())}>
                      <div class="px-1 overflow-hidden text-gray-700">
                        <div class="px-2 py-1 my-1 text-xs font-medium text-neutral-500 capitalize">{item.group}</div>
                      </div>
                    </Show>
                    <SearchItem item={item} index={index()} click={() => execute(index())} />
                  </div>
                )}
              </For>
            </div>
          </div>
        </div>
      </div>
    </Show>
  )
}