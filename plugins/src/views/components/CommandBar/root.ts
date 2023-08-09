import { createEffect, createRoot, createSignal } from 'solid-js';

const root = createRoot(() => {
  const [search, setSearch] = createSignal('');
  const [activeIndex, setActiveIndex] = createSignal(0);
  const [visible, setVisible] = createSignal(true);

  createEffect(() => {
    if (search().length === 0) {
      setActiveIndex(0);
    }
  });

  return {
    search, setSearch,
    activeIndex, setActiveIndex,
    visible, setVisible,
  }
});

export const useRoot = () => root;