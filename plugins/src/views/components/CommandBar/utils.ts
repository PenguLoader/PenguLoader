import { Accessor, createEffect, createSignal, onCleanup } from 'solid-js';

export function evaluate(value?: string | (() => string)) {
  if (typeof value === 'string') {
    return value;
  } else if (typeof value === 'function') {
    return value();
  } else {
    return '';
  }
}

export function useThrottledValue<T>(value: Accessor<T>, ms = 50) {
  if (ms == 0) return value;

  const [throttledValue, setThrottledValue] = createSignal(value());
  const [lastRan, setLastRan] = createSignal(Date.now());

  createEffect(() => {
    const timeout = setTimeout(() => {
      setThrottledValue(value);
      setLastRan(Date.now());
    }, lastRan() - (Date.now() - ms));

    onCleanup(() => clearTimeout(timeout));
  });

  return throttledValue;
}