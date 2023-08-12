import { Show, onMount } from 'solid-js';
import { useRoot } from './root';
import { SearchBar } from './SearchBar';
import { SearchResults } from './SearchResults';
import { VisualState } from './types';
import { Animator } from './Animator';

export function CommandBar() {

  const { hidden, setVisualState } = useRoot();

  onMount(() => {
    window.addEventListener('keydown', e => {
      if (e.ctrlKey && e.code === 'KeyK' && hidden()) {
        e.preventDefault();
        setVisualState(VisualState.AnimatingIn);
      }
    });
  });

  const backdropClick = () => {
    setVisualState(VisualState.AnimatingOut);
  };

  return (
    <Show when={!hidden()}>
      <div class="fixed top-0 left-0 z-[99] flex items-center justify-center w-screen h-screen">
        <div
          class="absolute inset-0 w-full h-full bg-black bg-opacity-40"
          onClick={backdropClick}
        />
        <div class="flex min-h-[370px] justify-center w-full max-w-xl items-start relative">
          <Animator>
            <div class="box-border flex flex-col w-full h-full overflow-hidden
           bg-white rounded-md shadow-md bg-opacity-90 drop-shadow-md backdrop-blur-sm">
              <SearchBar />
              <SearchResults />
            </div>
          </Animator>
        </div>
      </div>
    </Show>
  )
}