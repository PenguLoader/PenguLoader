import { Component } from 'solid-js'
import { LoaderIcon } from '~/components/Icons'

export const SplashPage: Component = () => {
  return (
    <div class="flex justify-center items-center h-screen">
      <div class="flex flex-col items-center gap-4">
        <img class="size-32 rounded-full pointer-events-none" src="/pengu-xl.jpg" />
        {/* <h2 class="text-3xl font-semibold text-white">Pengu Loader</h2> */}
        <p class="text-lg text-slate-200 flex items-center">
          ✨ Build your unmatched <img class="mx-1 size-6 inline-block" src="/lol-logo.png" /> Client
        </p>
        <span class="animate-spin">
          <LoaderIcon thickness={1} />
        </span>
      </div>
    </div>
  )
}