import { For, Show, createEffect, createMemo, on, onCleanup, onMount } from 'solid-js';
import { useRoot } from './root';
import Fuse from 'fuse.js';
import { Action, VisualState } from './types';
import { SearchItem } from './SearchItem';

export function SearchResults() {

  let containerRef: HTMLDivElement;
  const { search, actions, activeIndex, setActiveIndex, setVisualState, hidden } = useRoot();

  const filteredItems = createMemo(() => {
    if (search().length === 0) {
      return actions()
        .filter(item => !item.hidden);
    }

    const fuse = new Fuse(actions(), {
      distance: 200,
      threshold: 0.4,
      includeScore: true,
      keys: ['name', 'tags', 'group']
    });

    return fuse.search(search())
      .filter(item => !item.item.hidden || (item.item.hidden && item.score! > 0))
      .sort((a, b) => b.score! - a.score!)
      .map(item => item.item);
  });

  function shouldShowCategory(item: Action, index: number) {
    if (index == 0) return true;
    if (typeof filteredItems()[index - 1] === 'undefined') return false;
    return item.group !== filteredItems()[index - 1].group;
  }

  function scrollToActiveItem() {
    if (hidden() || !containerRef) return;
    const activeElement: HTMLElement = containerRef.querySelector(`div[data-index="${activeIndex()}"]`)!;
    if (!activeElement) return;
    const newScrollPos = (activeElement.offsetTop + activeElement.offsetHeight) - containerRef.offsetHeight;
    if (newScrollPos > 0) {
      containerRef.scrollTop = newScrollPos;
    } else {
      containerRef.scrollTop = 0;
    }
  }

  function execute(index: number) {
    if (hidden()) return;
    const item = filteredItems()[index];
    setActiveIndex(index);
    setVisualState(VisualState.AnimatingOut);
    if (typeof item.perform === 'function') {
      item.perform(item.id);
    }
  }

  const handleKeyDown = (e: KeyboardEvent) => {
    if (e.isComposing) return;
    if (e.key === 'ArrowUp') {
      e.preventDefault();
      setActiveIndex(index => index && index - 1);
      scrollToActiveItem();
    } else if (e.key === 'ArrowDown') {
      e.preventDefault();
      setActiveIndex(index => index < filteredItems().length - 1 ? index + 1 : index);
      scrollToActiveItem();
    } else if (e.key === 'Enter') {
      e.preventDefault();
      execute(activeIndex());
    } else if (e.code === 'Escape') {
      e.preventDefault();
      setVisualState(VisualState.AnimatingOut);
    }
  };

  onMount(() => {
    createEffect(on(filteredItems, () => {
      setActiveIndex(0);
      scrollToActiveItem();
    }));

    window.addEventListener('keydown', handleKeyDown);
    onCleanup(() => window.removeEventListener('keydown', handleKeyDown));
  });

  return (
    <div
      ref={containerRef!}
      class="max-h-[320px] overflow-y-auto overflow-x-hidden scroll-smooth"
    >
      <For each={filteredItems()}>
        {(item, index) => (
          <div class="pb-1 space-y-1">
            <Show when={shouldShowCategory(item, index())}>
              <div class="px-1 overflow-hidden text-gray-700">
                <div class="px-2 py-1 my-1 text-xs font-medium text-neutral-500 capitalize">
                  {item.group}
                </div>
              </div>
            </Show>
            <SearchItem item={item} index={index()} click={() => execute(index())} />
          </div>
        )}
      </For>
    </div>
  )
}