import { createMemo, createRoot, createSignal } from 'solid-js';
import { VisualState } from './types';
import { DEFAULT_ACTIONS } from './data';

const root = createRoot(() => {
  const actions = createMemo(() => DEFAULT_ACTIONS);

  const [search, setSearch] = createSignal('');
  const [activeIndex, setActiveIndex] = createSignal(0);
  const [visualState, setVisualState] = createSignal(VisualState.Hidden);
  const hidden = createMemo(() => visualState() === VisualState.Hidden);

  return {
    actions,
    search, setSearch,
    activeIndex, setActiveIndex,
    visualState, setVisualState, hidden,
  }
});

export const useRoot = () => root;