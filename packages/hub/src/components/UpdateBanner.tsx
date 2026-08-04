import { Component, createSignal, Show } from 'solid-js'
import { Updater } from '~/lib/updater'
import { Shell } from '~/lib/shell'

/**
 * Fixed-bottom notification when an update is available. Reads
 * {@link Updater.available} so any check (auto-on-launch from App.tsx or
 * the manual button in Settings) lights it up without re-fetching.
 *
 * Layout:
 *   - Positioning belongs to {@link Banners}, which fixes the whole stack
 *     to the bottom — this renders as a plain block so a second banner
 *     stacks above it instead of covering it.
 *   - `bg-primary text-primary-foreground` — the theme's accent green
 *     against its dark-on-green foreground, matching Button "default".
 *
 * Dismiss is session-only — clearing on next launch is intentional so a
 * persistent stale skip-state can't strand a user on an old version
 * forever. Skip-this-version persistence can land later if the cadence
 * picks up.
 */
export const UpdateBanner: Component = () => {
  const [dismissed, setDismissed] = createSignal(false)

  const open = () => {
    const info = Updater.available()
    if (info) Shell.openLink(info.url)
  }

  return (
    <Show when={Updater.available() && !dismissed()}>
      <div class="bg-primary text-primary-foreground shadow-lg animate-in slide-in-from-bottom duration-300">
        <div class="flex items-center justify-between gap-3 px-4 py-2">
          <div class="text-sm">
            <span class="font-semibold">Pengu {Updater.available()!.tag}</span> is available.
          </div>
          <div class="flex items-center gap-2">
            <button
              class="text-sm font-medium underline underline-offset-2 hover:opacity-80"
              onClick={open}
              tabIndex={-1}
            >
              View release
            </button>
            <button
              class="text-primary-foreground/70 hover:text-primary-foreground p-1 -m-1"
              onClick={() => setDismissed(true)}
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
    </Show>
  )
}
