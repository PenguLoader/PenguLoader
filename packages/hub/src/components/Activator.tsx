import { Component, createSignal, onMount, Show } from 'solid-js'
import { Dynamic } from 'solid-js/web'
import { CoreModule } from '../lib/core-module'
import { BoltIcon, PowerIcon } from './Icons'
import { useTippy } from '../lib/utils'

/**
 * Activation toggle that lives in the appbar. Two visual states:
 *   - idle:  a `w-12 h-full` rect mirroring the Store / Settings command
 *            buttons so the appbar reads as a uniform strip.
 *   - hover: an emerald-bordered pill with the icon + "Activate" / "READY"
 *            label.
 *
 * The expand/collapse is driven by a JS `expanded` signal (mouseenter /
 * mouseleave), not the CSS `:hover` pseudo-class. The reason: `alert()`
 * freezes the event loop the instant it's called, so a pure-CSS hover
 * implementation leaves the pill stuck mid-expansion behind the modal —
 * visibly overlapping the adjacent Store icon. By owning the state in JS
 * we can force-collapse and await the transition before any blocking
 * dialog fires (see {@link Activator.collapseAndWait}).
 *
 * Source of truth for activation state:
 *   - initial state: `pengu.activation.isActive()` at mount.
 *   - daemon updates: `window` 'activation:stateChanged' events emitted from
 *     C# (RCS WAMP detect on macOS, post-toggle confirm on Windows).
 */
const TRANSITION_MS = 200

export const Activator: Component = () => {
  const [loading, setLoading] = createSignal(true)
  const [active, setActive] = createSignal(false)
  const [expanded, setExpanded] = createSignal(false)

  /**
   * Collapse the pill and wait the transition out before returning. Call
   * before any blocking dialog (`alert`, `confirm`) so the pill doesn't
   * freeze in the expanded state and overlap adjacent appbar buttons.
   */
  const collapseAndWait = async () => {
    setExpanded(false)
    await new Promise<void>(r => setTimeout(r, TRANSITION_MS + 20))
  }

  const activate = async () => {
    if (loading()) return
    setLoading(true)
    try {
      if (!await CoreModule.exists()) {
        await collapseAndWait()
        // TODO(overlay): replace browser alert with the in-app message overlay
        // once the component lands. Native dialogs are out of scope for app/.
        alert('Failed to perform activation, the core module is not found.')
        return
      }
      const nextActive = !active()
      const { activated, error } = await CoreModule.doActivate(nextActive)
      if (error) {
        await collapseAndWait()
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
      onMouseEnter={() => setExpanded(true)}
      onMouseLeave={() => setExpanded(false)}
      aria-busy={loading()}
      aria-checked={active()}
      data-expanded={expanded()}
      class="
        relative group flex items-center justify-center h-full
        w-12 data-[expanded=true]:w-auto data-[expanded=true]:px-2
        transition-all duration-200 ease-out
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
        flex items-center gap-1 h-7 px-2
        border border-transparent rounded-full
        bg-transparent
        transition-all duration-200 ease-out
        group-data-[expanded=true]:px-3
        group-data-[expanded=true]:border-foreground/25
        group-data-[expanded=true]:bg-foreground/5
        group-aria-checked:group-data-[expanded=true]:border-primary
        group-aria-checked:group-data-[expanded=true]:bg-primary/5
      ">
        {/* Icon stays primary-tinted in every state. Power (inactive) and Bolt
            (active) both read as a "go" affordance, so the green belongs to
            the icon itself rather than the surrounding pill chrome. Keeps the
            inactive expanded pill readable as gray without dulling the icon. */}
        <span class="text-primary">
          <Dynamic component={active() ? BoltIcon : PowerIcon} size={16} thickness={2.5} />
        </span>
        <span class="
          overflow-hidden whitespace-nowrap text-sm font-semibold
          max-w-0 ml-0 group-data-[expanded=true]:max-w-20 group-data-[expanded=true]:ml-1
          text-foreground/80
          group-aria-checked:text-primary
          transition-all duration-200 ease-out
        ">{active() ? 'READY' : 'Activate'}</span>
      </span>
    </button>
  )
}
