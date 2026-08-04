import { Component, createSignal, onCleanup, onMount, Show } from 'solid-js'
import { CoreModule } from '~/lib/core-module'
import { type BootStubState } from '~/lib/pengu'
import { useRoot } from '~/lib/root'

/**
 * Warns when the machine-wide boot is in a state that stops Pengu working —
 * or, in the worst case, stops League working.
 *
 * The boot never blocks the client: if it can't verify a build it launches
 * League anyway and skips injection. That is the right call — losing plugins
 * is an inconvenience, failing to start the game is not — but it means most
 * boot problems are indistinguishable from Pengu quietly not working. Nothing
 * else in the UI can show them.
 *
 * Only rendered from {@link Banners} on the main page, which is gated behind
 * the welcome tour, so none of this competes with onboarding.
 */

type BootIssue = {
  /** Dismissal is per-issue, so waving away the mild one can't bury a worse. */
  key: string
  headline: string
  detail: string
}

/**
 * Worst first. Exactly one banner shows, and it is the most severe thing true
 * right now.
 */
function diagnose(boot: BootStubState | null, activated: boolean): BootIssue | null {
  if (!boot) return null

  // The only state here that can break League itself. Windows is redirecting
  // LeagueClientUx.exe through a boot that no longer exists, so the client may
  // not start at all — and the usual cause is Defender quarantining it, which
  // gives the user no indication anything happened.
  if (boot.wired && !boot.installed) {
    return {
      key: 'missing',
      headline: 'League may not start.',
      detail: 'Windows is set to launch League through a Pengu file that is missing. '
        + 'Activating Pengu again puts it back.',
    }
  }

  // The boot ran, refused the runtime it was pointed at, and started League
  // without it. Gated on being activated because the boot clears this on its
  // next good launch but deactivating doesn't, so an old refusal would
  // otherwise nag someone who has deliberately turned Pengu off.
  if (activated && boot.lastRefusal) {
    return {
      key: 'refused',
      headline: 'League started without plugins.',
      detail: boot.lastRefusal,
    }
  }

  // Not wired at all, whether or not the file is present: nothing will load.
  // Covers a fresh install, an interrupted uninstall, and anything else that
  // claimed the registry entry.
  if (!boot.wired) {
    return {
      key: 'not-wired',
      headline: boot.installed
        ? 'Pengu is installed but not in use.'
        : 'Pengu is not set up to start with League.',
      detail: 'Activate Pengu to connect it to the League client. '
        + 'This asks for administrator permission once.',
    }
  }

  return null
}

export const BootBanner: Component = () => {
  const { settings } = useRoot()
  const [state, setState] = createSignal<BootStubState | null>(null)
  const [activated, setActivated] = createSignal(false)
  const [dismissed, setDismissed] = createSignal('')

  const refresh = async () => {
    const [boot, on] = await Promise.all([CoreModule.bootState(), CoreModule.isActivated()])
    setState(boot)
    setActivated(on)
  }

  onMount(() => {
    refresh()

    // The boot writes its log when the client launches, which is usually while
    // the hub is closed or in the background — so a mount-time read alone
    // misses the case that matters most. Refreshing on focus covers coming
    // back to the hub after a launch, without a timer.
    window.addEventListener('activation:stateChanged', refresh)
    window.addEventListener('focus', refresh)
    onCleanup(() => {
      window.removeEventListener('activation:stateChanged', refresh)
      window.removeEventListener('focus', refresh)
    })
  })

  const issue = () => {
    const found = diagnose(state(), activated())
    return found && found.key !== dismissed() ? found : null
  }

  const openDetails = () => {
    setDismissed(issue()?.key ?? '')
    settings.show()
  }

  return (
    <Show when={issue()}>
      {(current) => (
        <div class="bg-warning text-warning-foreground shadow-lg animate-in slide-in-from-bottom duration-300">
          <div class="flex items-center justify-between gap-3 px-4 py-2">
            <div class="flex items-center gap-2 min-w-0 text-sm">
              <svg width="14" height="14" viewBox="0 0 16 16" fill="currentColor" class="shrink-0">
                <path d="M8 1.5 15.5 14.5H0.5L8 1.5Zm0 4a.75.75 0 0 0-.75.75v3.5a.75.75 0 0 0 1.5 0v-3.5A.75.75 0 0 0 8 5.5Zm0 6a.9.9 0 1 0 0 1.8.9.9 0 0 0 0-1.8Z" />
              </svg>
              <span class="font-semibold whitespace-nowrap">{current().headline}</span>
              {/* min-w-0 above is what lets this truncate inside the flex row;
                  the full text stays reachable as a tooltip. */}
              <span class="truncate opacity-90" title={current().detail}>{current().detail}</span>
            </div>
            <div class="flex items-center gap-2 shrink-0">
              <button
                class="text-sm font-medium underline underline-offset-2 hover:opacity-80"
                onClick={openDetails}
                tabIndex={-1}
              >
                Details
              </button>
              <button
                class="text-warning-foreground/70 hover:text-warning-foreground p-1 -m-1"
                onClick={() => setDismissed(current().key)}
                tabIndex={-1}
                aria-label="Dismiss"
                title="Dismiss"
              >
                <svg width="12" height="12" viewBox="0 0 10 10" fill="currentColor">
                  <polygon points="10.2,0.7 9.5,0 5.1,4.4 0.7,0 0,0.7 4.4,5.1 0,9.5 0.7,10.2 5.1,5.8 9.5,10.2 10.2,9.5 5.8,5.1" />
                </svg>
              </button>
            </div>
          </div>
        </div>
      )}
    </Show>
  )
}
