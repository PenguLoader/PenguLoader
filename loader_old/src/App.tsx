import { createSignal, onMount, Show } from 'solid-js'
import { Config } from './lib/config'
import { WelcomePage } from './pages/WelcomePage'
import { Appbar } from './components/Appbar'
import { MainPage } from './pages/MainPage'

import './App.css'
import 'tippy.js/dist/tippy.css'

function App() {
  const [ready, setReady] = createSignal(false)
  const [welcome, setWelcome] = createSignal(true)

  onMount(async () => {
    setWelcome(!await Config.load())
    setReady(true)
  })

  return (
    <div class="h-screen flex flex-col">
      <div class="blur-[140px] h-[10rem] max-w-[40rem] absolute top-[10rem] z-10 pointer-events-none w-[-webkit-fill-available]">
        <div class="w-full h-full bg-[linear-gradient(97.62deg,rgba(0,71,225,0.22),rgba(26,214,255,0.32),rgba(0,220,130,0.42))]">
        </div>
      </div>
      <Show when={ready()}>
        <Appbar isHome={!welcome()} />
        <Show
          when={!welcome()}
          fallback={<WelcomePage onDone={() => setWelcome(false)} />}
        >
          <MainPage />
        </Show>
      </Show>
    </div>
  )
}

export default App