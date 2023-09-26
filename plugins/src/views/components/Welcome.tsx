import { Show, createSignal, onMount } from 'solid-js';
import penguLogo from '../assets/pengu.jpg';
import { toast } from './Toaster';
import { _t } from '../i18n';

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

  if (!welcome) {
    onMount(() => {
      toast.success(_t('active_status'), {
        position: 'bottom-left',
        duration: 7000
      });
    });
  }

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