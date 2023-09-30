import { onMount } from 'solid-js';
import { useRoot } from './root';
import { _t } from '../../i18n';

export function SearchBar() {
  let input: HTMLInputElement;
  const { search, setSearch, setActiveIndex } = useRoot();

  onMount(() => {
    setSearch('');
    setActiveIndex(0);
    input.focus();
  });

  const onKeyDown = (e: KeyboardEvent) => {
    if (e.key === 'ArrowUp') {
      e.preventDefault();
    }
  };

  return (
    <div class="flex items-center px-3 border-b border-gray-300 bg-white/90">
      <svg
        class="w-4 h-4 mr-0 text-neutral-400 shrink-0"
        xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24"
        fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"
      >
        <circle cx="11" cy="11" r="8"></circle>
        <line x1="21" x2="16.65" y1="21" y2="16.65"></line>
      </svg>
      <input
        ref={input!}
        type="text" value={search()}
        onKeyDown={onKeyDown}
        onInput={e => setSearch(e.target.value)}
        onBlur={() => setTimeout(() => input.focus(), 50)}
        class="flex w-full px-2 py-3 text-sm bg-transparent border-none rounded-md outline-none
        placeholder:text-neutral-400 h-11 disabled:cursor-not-allowed disabled:opacity-50"
        placeholder={_t('cmdbar_type_hint')} autocomplete="off" autocorrect="off" spellcheck={false}
      />
    </div>
  )
}