import { Component, createSignal, Match, onCleanup, onMount, Show, Switch } from 'solid-js'
import { Config, useConfig } from '~/lib/config'
import { CheckOption, OptionSet, RadioOption } from './templates'
import { ComboBox } from '~/components/ui/ComboBox'
import { Startup } from '~/lib/startup'
import { pengu, type BootStubState } from '~/lib/pengu'
import { CoreModule } from '~/lib/core-module'
import { Shell } from '~/lib/shell'
import { Updater } from '~/lib/updater'
import { useI18n } from '~/lib/i18n'

/**
 * Update controls. Two affordances:
 *   - Auto-check toggle, persisted in `app.auto_update_check`. App.tsx fires
 *     a check on launch when this is true; the result lands in
 *     `Updater.available()` so the status row below mirrors it.
 *   - Manual "Check for update" button — overrides the auto path so the
 *     user can re-check without restarting (e.g. they just bumped past a
 *     release on the page).
 *
 * Status row consumes `Updater.available()` / `Updater.error()` so a
 * launch-time check shows up here without re-fetching when the tab opens.
 */
const UpdateSettings: Component = () => {
  const { app } = useConfig()
  const [checking, setChecking] = createSignal(false)
  const [checkedOnce, setCheckedOnce] = createSignal(false)

  const checkNow = async () => {
    if (checking()) return
    setChecking(true)
    try {
      await Updater.check()
    } catch {
      /* Updater.error() already populated */
    } finally {
      setChecking(false)
      setCheckedOnce(true)
    }
  }

  const openRelease = () => {
    const info = Updater.available()
    if (info) Shell.openLink(info.url)
  }

  return (
    <OptionSet name="App Updates">
      <CheckOption
        caption="Auto check for updates"
        message="Check the GitHub releases page for a newer Pengu version on launch."
        checked={app.auto_update_check()}
        onChange={app.auto_update_check}
      />
      <div class="flex items-center gap-3 flex-wrap">
        <button
          class="inline-flex gap-1 items-center text-sm border border-foreground/10 rounded-sm px-3 py-1 hover:bg-foreground hover:text-background aria-busy:opacity-60 aria-busy:pointer-events-none"
          tabIndex={-1}
          onClick={checkNow}
          aria-busy={checking()}
        >
          {checking() ? 'Checking…' : 'Check for update'}
        </button>
        <Switch>
          <Match when={Updater.available()}>
            <span class="text-sm text-muted-foreground">
              <span class="text-primary font-semibold">{Updater.available()!.tag}</span> available —{' '}
              <a class="underline cursor-pointer text-foreground/80 hover:text-foreground" onClick={openRelease}>view release</a>
            </span>
          </Match>
          <Match when={Updater.error()}>
            <span class="text-sm text-destructive">Failed: {Updater.error()}</span>
          </Match>
          <Match when={checkedOnce() && !Updater.available() && !Updater.error()}>
            <span class="text-sm text-muted-foreground">You're on the latest version (v{window.appVersion}).</span>
          </Match>
        </Switch>
      </div>
    </OptionSet>
  )
}

/**
 * The machine-wide boot (Windows).
 *
 * Kept apart from the Activate toggle on purpose, because they are different
 * things and cost different amounts. Activating is a per-user file write with
 * no prompt and no effect on anyone else. The boot is installed once for the
 * whole machine, needs elevation, and removing it stops every account getting
 * Pengu — so it lives here, behind a confirm, rather than next to a switch
 * people flip casually.
 *
 * Also where a refusal surfaces. The boot never blocks the client: if it can't
 * verify a build it launches League anyway and skips the plugin runtime. That
 * is the right behaviour, but it means a signing or rollout mistake is
 * indistinguishable from Pengu quietly not working unless we say so.
 */
const BootSettings: Component = () => {
  const [state, setState] = createSignal<BootStubState | null>(null)
  const [busy, setBusy] = createSignal(false)
  const [confirming, setConfirming] = createSignal(false)
  const [error, setError] = createSignal('')

  const refresh = async () => setState(await CoreModule.bootState())

  // The first activation is also what installs the boot, and that happens over
  // in the Activator — so mount-time state goes stale the moment the user
  // flips the switch. Ride the same `activation:stateChanged` event the
  // Activator listens to, which the host emits after any successful change,
  // including removeBoot.
  onMount(() => {
    refresh()
    window.addEventListener('activation:stateChanged', refresh)
    onCleanup(() => window.removeEventListener('activation:stateChanged', refresh))
  })

  const remove = async () => {
    if (busy()) return
    setBusy(true)
    setError('')
    try {
      const result = await CoreModule.removeBoot()
      if (!result.ok) setError(result.error)
      await refresh()
    } finally {
      setBusy(false)
      setConfirming(false)
    }
  }

  return (
    <Show when={state()}>
      {(boot) => (
        <OptionSet name="System Boot">
          <div class="space-y-3">
            <p class="text-sm text-muted-foreground">
              <Switch fallback={<>Not installed. It is set up the first time you activate Pengu.</>}>
                <Match when={boot().installed && boot().wired}>
                  Installed{boot().installedVersion ? ` (v${boot().installedVersion})` : ''} and running when League starts.
                  Turning Pengu on and off doesn't need this to change.
                </Match>
                <Match when={boot().installed && !boot().wired}>
                  Installed but not in use — League isn't set up to start it. Activating Pengu will reconnect it.
                </Match>
              </Switch>
            </p>

            <Show when={boot().updateAvailable}>
              <p class="text-sm text-muted-foreground">
                This version of Pengu ships a newer boot
                {boot().shippedVersion ? ` (v${boot().shippedVersion})` : ''}. It updates the next time you activate,
                which will ask for administrator permission once.
              </p>
            </Show>

            <Show when={boot().lastRefusal}>
              <p class="text-sm text-destructive">
                League last started without plugins: {boot().lastRefusal}
              </p>
            </Show>

            <Show when={boot().installed}>
              <Show
                when={confirming()}
                fallback={
                  <button
                    class="inline-flex gap-1 items-center text-sm border border-foreground/10 rounded-sm px-3 py-1 hover:bg-destructive hover:text-background"
                    tabIndex={-1}
                    onClick={() => setConfirming(true)}
                  >
                    Remove system boot
                  </button>
                }
              >
                <div class="space-y-2">
                  <p class="text-sm">
                    This removes Pengu from League's startup for <b>every account on this computer</b>, and asks for
                    administrator permission. To just turn Pengu off for yourself, use the Activate switch instead.
                  </p>
                  <div class="flex items-center gap-3">
                    <button
                      class="text-sm border border-destructive/40 text-destructive rounded-sm px-3 py-1 hover:bg-destructive hover:text-background aria-busy:opacity-60 aria-busy:pointer-events-none"
                      tabIndex={-1}
                      onClick={remove}
                      aria-busy={busy()}
                    >
                      {busy() ? 'Removing…' : 'Remove it'}
                    </button>
                    <button
                      class="text-sm text-muted-foreground hover:text-foreground"
                      tabIndex={-1}
                      onClick={() => setConfirming(false)}
                    >
                      Cancel
                    </button>
                  </div>
                </div>
              </Show>
            </Show>

            <Show when={error()}>
              <p class="text-sm text-destructive">{error()}</p>
            </Show>
          </div>
        </OptionSet>
      )}
    </Show>
  )
}

const LaunchSettings: Component = () => {
  const [startup, setSatrtup] = createSignal(false)

  const toggleStartup = async () => {
    let enable = !await Startup.isEnabled()
    await Startup.setEnable(enable)
    setSatrtup(enable)
  }

  onMount(async () => {
    setSatrtup(await Startup.isEnabled())
  })

  return (
    <OptionSet name="Launch Settings">
      <CheckOption
        caption="Run on startup"
        message="Automatically run Pengu when your computer starts."
        checked={startup()}
        onClick={toggleStartup}
      />
    </OptionSet>
  )
}

export const TabPengu: Component = () => {

  const { app } = useConfig()
  const i18n = useI18n()

  const changePluginsDir = async () => {
    const dir = await pengu.host.pickFolder(Config.basePath())
    if (typeof dir === 'string') {
      await app.plugins_dir(dir)
    }
  }

  // Language picker mirrors the WelcomePage step-1 control. Same setter
  // path: switch the runtime i18n instance, then persist to config.
  const selectLang = async (id: string) => {
    i18n.switchTo(id)
    await app.language(id)
  }

  return (
    <div class="space-y-4">

      <OptionSet name="Language">
        <ComboBox
          items={i18n.languages}
          selected={app.language()}
          onSelect={selectLang}
        />
      </OptionSet>

      <OptionSet name="Plugins Folder">
        <span
          class="block text-base text-neutral-200 px-3 py-1 hover:bg-neutral-400/20 rounded-md"
          onClick={changePluginsDir}>
          {app.plugins_dir() || Config.basePath('plugins')}
        </span>
      </OptionSet>

      <Show when={window.isMac}>
        <LaunchSettings />
      </Show>

      <OptionSet name="Activation Mode">
        <Show when={!window.isMac}>
          {/* Windows: Universal (IFEO) only. OnDemand was considered as a
              second mode but dropped — IFEO is strictly more reliable on
              Windows (no daemon required, survives reboots, kernel-level
              redirect). The radio stays as a single visible option for
              clarity rather than collapsing to a label. */}
          <RadioOption
            caption="Universal"
            message="Apply to all League Clients. Requires UAC once at install; survives across launches."
            checked
            disabled
          />
        </Show>
        <Show when={window.isMac}>
          <RadioOption
            caption="Universal"
            message="Apply to all League Clients. You have to keep Pengu running in background."
            disabled
            checked
          />
        </Show>
      </OptionSet>

      <Show when={!window.isMac}>
        <BootSettings />
      </Show>

      <UpdateSettings />

    </div>
  )
}
