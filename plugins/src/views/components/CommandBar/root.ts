import { createMemo, createRoot, createSignal, on } from 'solid-js';
import { DEFAULT_ACTIONS } from './data';

const ACTION_LIST = [...DEFAULT_ACTIONS];

export enum VisualState {
  Hidden = 0,
  AnimatingIn,
  Showing,
  AnimatingOut,
}

const root = createRoot(() => {
  const [updated, triggerUpdate] = createSignal(undefined, { equals: false });
  const actions = createMemo(on(updated, () => ACTION_LIST), ACTION_LIST, { equals: false });

  const [search, setSearch] = createSignal('');
  const [activeIndex, setActiveIndex] = createSignal(0);
  const [visualState, setVisualState] = createSignal(VisualState.Hidden);
  const hidden = createMemo(() => visualState() === VisualState.Hidden);

  function addAction(item: Action) {
    if (typeof item !== 'object' || !item.name) {
      console.warn('[CommandBar] Action item should be an object with `name` and `perform` props.')
      return;
    }

    const action = { ...item };

    if (!action.group || typeof action.group === 'string') {
      action.group = 'uncategorized';
    }

    ACTION_LIST.push(action);
    triggerUpdate();
  }

  window.CommandBar = {
    addAction,
    show: () => setVisualState(VisualState.AnimatingIn),
    update: () => triggerUpdate(),
  }

  return {
    actions,
    search, setSearch,
    activeIndex, setActiveIndex,
    visualState, setVisualState, hidden,
  }
});

export const useRoot = () => root;