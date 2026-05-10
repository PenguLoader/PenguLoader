import { Component, createSignal, onMount, Show } from 'solid-js'
import { Dynamic } from 'solid-js/web'
import { CoreModule } from '../lib/core-module'
import { BoltIcon, PowerIcon } from './Icons'
import { useTippy } from '../lib/utils'

/**
 * Activation toggle that lives in the appbar. Its footprint intentionally
 * mirrors the Store / Settings command buttons: fixed `w-12 h-full`, no
 * expanding hover surface. Earlier pill-style expansion could visually cover
 * or churn at the boundary with the adjacent Plugin Store command.
 *
 * Source of truth for activation state:
 *   - initial state: `pengu.activation.isActive()` at mount.
 *   - daemon updates: `window` 'activation:stateChanged' events emitted from
 *     C# (RCS WAMP detect on macOS, post-toggle confirm on Windows).
 */
export const Activator: Component = () => {
  const [loading, setLoading] = createSignal(true)
  const [active, setActive] = createSignal(false)

  const activate = async () => {
    if (loading()) return
    setLoading(true)
    try {
      if (!await CoreModule.exists()) {
        // TODO(overlay): replace browser alert with the in-app message overlay
        // once the component lands. Native dialogs are out of scope for app/.
        alert('Failed to perform activation, the core module is not found.')
        return
      }
      const nextActive = !active()
      const { activated, error } = await CoreModule.doActivate(nextActive)
      if (error) {
        alert(`Failed to perform activation, got error:\n${error}`)
      } else if (activated === nextActive) {
        setActive(activated)
      }
    } finally {
      setLoading(false)
    }
  }

  onMount(async () => {
    setActive(await CoreModule.isActivated())
    setLoading(false)
    window.addEventListener('activation:stateChanged', (e) => {
      const detail = (e as CustomEvent<{ active: boolean }>).detail
      if (detail && typeof detail.active === 'boolean') {
        setActive(detail.active)
      }
    })
  })

  return (
    <button
      type="button"
      onClick={activate}
      aria-busy={loading()}
      aria-checked={active()}
      class="
        relative group flex h-full w-12 items-center justify-center
        hover:bg-foreground/15
        aria-busy:opacity-60
      "
    >
      {/* Tooltip targets. useTippy captures content at mount, so we keep
          two static spans and toggle which one is in the DOM via Show.
          Each is an absolute overlay on the button — clicks bubble to the
          parent's onClick; tippy uses the span for its mouseenter listener.
          When active() flips, the unmounted branch's tippy instance is
          destroyed by useTippy's onCleanup. */}
      <Show when={!active()}>
        <span class="absolute inset-0" ref={useTippy('Click to activate Pengu')} />
      </Show>
      <Show when={active()}>
        <span class="absolute inset-0" ref={useTippy('Click to deactivate Pengu')} />
      </Show>
      <span class="
        flex size-7 items-center justify-center
        border border-transparent rounded-full
        transition-colors duration-150 ease-out
        group-hover:border-foreground/25
        group-hover:bg-foreground/5
        group-aria-checked:border-primary/80
        group-aria-checked:bg-primary/10
      ">
        <span class="text-primary">
          <Dynamic component={active() ? BoltIcon : PowerIcon} size={16} thickness={2.5} />
        </span>
      </span>
    </button>
  )
}
