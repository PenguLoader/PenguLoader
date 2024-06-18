import { Show, createSignal, onMount } from 'solid-js';
import penguLogo from '../assets/pengu.jpg';
import { toast } from './Toaster';
import { _t } from '../lib/i18n';
import { fetchUpdate } from '../lib/updater';

async function doCheckUpdate() {
  const update = await fetchUpdate();
  if (update === false) return;

  toast.custom((t) => {
    return (
      <div class={`${!t.visible && 'hidden'} relative w-[370px] bg-white shadow-lg rounded-lg pointer-events-auto ring-1 ring-black ring-opacity-5 overflow-hidden`}>
        <div class="p-2">
          <div class="flex items-start">
            <div class="flex-shrink-0 pt-[2px] text-gray-600">
              <svg width="24" height="24" viewBox="0 0 24 24" stroke-width="2" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
                <path stroke="none" d="M0 0h24v24H0z" fill="none"></path>
                <path d="M9 12h-3.586a1 1 0 0 1 -.707 -1.707l6.586 -6.586a1 1 0 0 1 1.414 0l6.586 6.586a1 1 0 0 1 -.707 1.707h-3.586v3h-6v-3z"></path>
                <path d="M9 21h6"></path>
                <path d="M9 18h6"></path>
              </svg>
            </div>
            <div class="ml-3 w-0 flex-1 pt-0.5">
              <p class="text-sm font-bold text-sky-500">{_t('update_available')} - {update.version}</p>
              <p class="mt-1 text-sm text-gray-700">{_t('update_hint')}</p>
            </div>
            <div class="ml-4 flex-shrink-0 flex">
              <button
                class="bg-white rounded-md inline-flex text-gray-400 hover:text-gray-500"
                onClick={() => toast.dismiss(t.id)}
              >
                <svg class="h-5 w-5" viewBox="0 0 20 20" fill="currentColor">
                  <path fill-rule="evenodd" d="M4.293 4.293a1 1 0 011.414 0L10 8.586l4.293-4.293a1 1 0 111.414 1.414L11.414 10l4.293 4.293a1 1 0 01-1.414 1.414L10 11.414l-4.293 4.293a1 1 0 01-1.414-1.414L8.586 10 4.293 5.707a1 1 0 010-1.414z" clip-rule="evenodd" />
                </svg>
              </button>
            </div>
          </div>
        </div>

      </div>
    )
  }, { duration: 30000, position: 'bottom-left' });
}

export function Welcome() {

  const welcome = window.DataStore?.get<boolean>('pengu-welcome', true) !== false;
  const [visible, show] = createSignal(welcome);

  const dontShowCheck = (e) => {
    const value = !e.currentTarget.value;
    window.DataStore?.set('pengu-welcome', value);
  };

  const hide = () => {
    show(false);
  };

  if (!welcome && !window.Pengu.silentMode) {
    onMount(() => {
      toast.success(_t('active_status'), {
        position: 'bottom-left',
        duration: 7000
      });
    });
  }

  onMount(doCheckUpdate);

  return (
    <Show when={visible()}>
      <div class="z-10 relative">
        <div class="fixed inset-0 bg-black bg-opacity-50 transition-opacity"></div>
        <div class="fixed inset-0 z-10 overflow-y-auto">
          <div class="flex min-h-full justify-center text-center items-center p-0">
            <div class="relative transform overflow-hidden rounded-lg bg-white text-left shadow-xl transition-all my-8 max-w-lg">
              <div class="bg-white px-4 pt-5 p-6 pb-4">
                <div class="sm:flex sm:items-start">
                  <div class="mx-auto flex flex-shrink-0 items-center justify-center h-10 w-10">
                    <img src={penguLogo} class="w-10 h-10 rounded" alt="" />
                  </div>
                  <div class="ml-4 mt-0 text-left">
                    <h3 class="text-base mt-0 font-semibold leading-6 text-gray-900">Pengu Loader</h3>
                    <div class="mt-2">
                      <div class="text-sm text-black">{_t('welcome_msg')}</div>
                      <div class="flex mt-5 space-x-1">
                        <a href="https://chat.pengu.lol/" target="_blank" rel="noreferrer" class="opacity-90 hover:opacity-100">
                          <img src="https://img.shields.io/discord/1069483280438673418?style=flat-square&logo=discord&logoColor=white&label=discord&color=5c5fff" alt="" />
                        </a>
                        <a href="https://pengu.lol/" target="_blank" rel="noreferrer" class="opacity-90 hover:opacity-100">
                          <img src="https://img.shields.io/badge/-pengu.lol-607080.svg?&style=flat-square&logo=gitbook&logoColor=white" alt="" />
                        </a>
                        <a href="https://github.com/PenguLoader/PenguLoader/" target="_blank" rel="noreferrer" class="opacity-90 hover:opacity-100">
                          <img src="https://img.shields.io/github/stars/PenguLoader/PenguLoader?style=flat-square&logo=github" alt="" />
                        </a>
                      </div>
                    </div>
                  </div>
                </div>
              </div>
              <div class="bg-gray-50 py-3 flex flex-row justify-between items-center px-6">
                <div class="flex items-center space-x-2">
                  <input type="checkbox" id="TxrO6Gew" onChange={dontShowCheck} class="h-4 w-4 rounded border-gray-300 outline-none" />
                  <label for="TxrO6Gew" class="text-sm font-medium text-gray-700">{_t('dont_show_again')}</label>
                </div>
                <button
                  onClick={hide}
                  type="button"
                  class="uppercase text-sm outline-none border-none bg-gray-300/60 px-3 py-1 text-gray-900 rounded hover:bg-gray-300 cursor-pointer"
                >Okay</button>
              </div>
            </div>
          </div>
        </div>
      </div>
    </Show>
  )
}