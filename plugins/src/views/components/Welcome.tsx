import { Show, createSignal } from 'solid-js';
import penguLogo from '../assets/pengu.jpg';

export function Welcome() {

  const [visible, show] = createSignal(true);

  return (
    <Show when={visible()}>
      <div class="z-10 relative">
        <div class="fixed inset-0 bg-black bg-opacity-50 transition-opacity"></div>
        <div class="fixed inset-0 z-10 overflow-y-auto">
          <div class="flex min-h-full justify-center p-4 text-center sm:items-center sm:p-0">
            <div class="relative transform overflow-hidden rounded-lg bg-white text-left shadow-xl transition-all sm:my-8 sm:w-full sm:max-w-lg">
              <div class="bg-white px-4 pb-4 pt-5 sm:p-6 sm:pb-4">
                <div class="sm:flex sm:items-start">
                  <div class="mx-auto flex h-12 w-12 flex-shrink-0 items-center justify-center sm:mx-0 sm:h-10 sm:w-10">
                    <img src={penguLogo} class="w-10 h-10 rounded" alt="" />
                  </div>
                  <div class="mt-3 sm:ml-4 sm:mt-0 sm:text-left">
                    <h3 class="text-base mt-0 font-semibold leading-6 text-gray-900" id="modal-title">Pengu Loader</h3>
                    <div class="mt-2">
                      <div class="text-sm text-black">
                        Hi Summoner, your installed plugins have been successfully loaded.
                        Join our community to get more 😎 awesome plugins and themes now 👇
                      </div>
                      <div class="flex my-3 space-x-1">
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
              <div class="bg-gray-50 px-4 py-3 flex flex-row justify-between items-center sm:px-6">
                <div class="flex items-center space-x-2">
                  <input type="checkbox" id="TxrO6Gew" class="h-4 w-4 rounded border-gray-300 outline-none" />
                  <label for="TxrO6Gew" class="text-sm font-medium text-gray-700">
                    Do not show again
                  </label>
                </div>
                <button
                  onClick={() => show(false)}
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