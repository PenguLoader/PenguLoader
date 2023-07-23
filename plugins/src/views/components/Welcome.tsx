import { Show, createSignal } from 'solid-js';
import penguLogo from '../assets/pengu.jpg';

export function Welcome() {

  const [visible, show] = createSignal(true);

  return (
    <Show when={visible()}>
      <div class="tw-z-10 tw-relative">
        <div class="tw-fixed tw-inset-0 tw-bg-black tw-bg-opacity-50 tw-transition-opacity"></div>
        <div class="tw-fixed tw-inset-0 tw-z-10 tw-overflow-y-auto">
          <div class="tw-flex tw-min-h-full tw-items-end tw-justify-center tw-p-4 tw-text-center sm:tw-items-center sm:tw-p-0">
            <div class="tw-relative tw-transform tw-overflow-hidden tw-rounded-lg tw-bg-white tw-text-left tw-shadow-xl tw-transition-all sm:tw-my-8 sm:tw-w-full sm:tw-max-w-lg">
              <div class="tw-bg-white tw-px-4 tw-pb-4 tw-pt-5 sm:tw-p-6 sm:tw-pb-4">
                <div class="sm:tw-flex sm:tw-items-start">
                  <div class="tw-mx-auto tw-flex tw-h-12 tw-w-12 tw-flex-shrink-0 tw-items-center tw-justify-center sm:tw-mx-0 sm:tw-h-10 sm:tw-w-10">
                    <img src={penguLogo} class="tw-w-10 tw-h-10 tw-rounded" alt="" />
                  </div>
                  <div class="tw-mt-3 tw-text-center sm:tw-ml-4 sm:tw-mt-0 sm:tw-text-left">
                    <h3 class="tw-text-base tw-mt-0 tw-font-semibold tw-leading-6 tw-text-gray-900" id="modal-title">Pengu Loader</h3>
                    <div class="tw-mt-2">
                      <p class="tw-text-sm tw-text-black">
                        Hi Summoner, your installed plugins have been successfully loaded.
                        Join our community to get more 😎 awesome plugins and themes now 👇
                      </p>
                      <p class="flex tw-space-x-1">
                        <a href="https://pengu.lol/" target="_blank" rel="noreferrer" class="tw-opacity-90 hover:tw-opacity-100">
                          <img height="28" src="https://img.shields.io/badge/-pengu.lol-607080.svg?&style=for-the-badge&logo=Authy&logoColor=white" alt="" />
                        </a>
                        <a href="https://chat.pengu.lol/" target="_blank" rel="noreferrer" class="tw-opacity-90 hover:tw-opacity-100">
                          <img height="28" src="https://img.shields.io/badge/-discord-5c5fff.svg?&style=for-the-badge&logo=Discord&logoColor=white" alt="" />
                        </a>
                        <a href="https://github.com/PenguLoader/PenguLoader/" target="_blank" rel="noreferrer" class="tw-opacity-90 hover:tw-opacity-100">
                          <img height="28" src="https://img.shields.io/badge/-github-1f2328.svg?&style=for-the-badge&logo=Github&logoColor=white" alt="" />
                        </a>
                      </p>
                    </div>
                  </div>
                </div>
              </div>
              <div class="tw-bg-gray-50 tw-px-4 tw-py-3 tw-flex tw-flex-row tw-justify-between tw-items-center sm:tw-px-6">
                <div class="tw-flex tw-items-center tw-space-x-2">
                  <input type="checkbox" id="TxrO6Gew" class="tw-h-4 tw-w-4 tw-rounded tw-border-gray-300 tw-outline-none" />
                  <label for="TxrO6Gew" class="tw-text-sm tw-font-medium tw-text-gray-700">
                    Do not show again
                  </label>
                </div>
                <button
                  onClick={() => show(false)}
                  type="button"
                  class="tw-uppercase tw-text-sm tw-outline-none tw-border-none tw-bg-gray-300/60 tw-px-3 tw-py-1 tw-text-gray-900 tw-rounded hover:tw-bg-gray-300 tw-cursor-pointer"
                >Okay</button>
              </div>
            </div>
          </div>
        </div>
      </div>
    </Show>
  )
}