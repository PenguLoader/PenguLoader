import { createRoot, createSignal } from 'solid-js';

const root = createRoot(() => {
  const [search, setSearch] = createSignal('');
  const [activeIndex, setActiveIndex] = createSignal(0);
  const [visible, setVisible] = createSignal(false);

  return {
    search, setSearch,
    activeIndex, setActiveIndex,
    visible, setVisible,
  }
});

export const useRoot = () => root;