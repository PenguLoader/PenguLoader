import { Component, createSignal, onMount, Show } from "solid-js";
import { dialog } from "@tauri-apps/api";
import { Config, useConfig } from "~/lib/config";
import { LeagueClient } from "~/lib/league-client";
import { CheckOption, OptionSet, RadioOption } from "./templates";
import { ActivationMode, CoreModule } from "~/lib/core-module";
import { Startup } from "~/lib/startup";
import { useI18n } from "~/lib/i18n";

const LaunchSettings: Component = () => {
  const [startup, setSatrtup] = createSignal(false);

  const toggleStartup = async () => {
    let enable = !(await Startup.isEnabled());
    await Startup.setEnable(enable);
    setSatrtup(enable);
  };

  onMount(async () => {
    setSatrtup(await Startup.isEnabled());
  });

  return (
    <OptionSet name="Launch Settings">
      <CheckOption
        caption="Run on startup"
        message="Automatically run Pengu when your computer starts."
        checked={startup()}
        onClick={toggleStartup}
      />
    </OptionSet>
  );
};

export const TabPengu: Component = () => {
  const i18n = useI18n();
  const { app } = useConfig();

  const changePluginsDir = async () => {
    const dir = await dialog.open({
      directory: true,
      defaultPath: Config.basePath(),
    });

    if (typeof dir === "string") {
      await app.plugins_dir(dir);
    }
  };

  const setActivationMode = async (mode: ActivationMode) => {
    if (await CoreModule.isActivated()) {
      await dialog.message(i18n.t("desactivate_before_activation_change"), {
        type: "warning",
      });
    } else {
      await app.activation_mode(mode);
    }
  };

  const changeLeagueDir = async () => {
    const dir = await dialog.open({
      directory: true,
    });

    if (typeof dir === "string") {
      if (await LeagueClient.validateDir(dir)) {
        await app.league_dir(dir);
      } else {
        await dialog.message(i18n.t("invalid_path"), { type: "warning" });
      }
    }
  };

  return (
    <div class="space-y-4">
      <OptionSet name={i18n.t("plugins_folder")}>
        <span
          class="block text-base text-neutral-200 px-3 py-1 hover:bg-neutral-400/20 rounded-md"
          onClick={changePluginsDir}
        >
          {app.plugins_dir() || "./plugins"}
        </span>
      </OptionSet>

      <Show when={!window.isMac}>
        <OptionSet
          name={i18n.t("lol_client_location")}
          disabled={app.activation_mode() === ActivationMode.Universal}
        >
          <span
            class="block text-base text-neutral-200 px-3 py-1 hover:bg-neutral-400/20 rounded-md"
            onClick={changeLeagueDir}
          >
            {app.league_dir() || `(${i18n.t("not_selected")})`}
          </span>
        </OptionSet>
      </Show>

      <Show when={window.isMac}>
        <LaunchSettings />
      </Show>

      <OptionSet name={i18n.t("activation_mode")}>
        <Show when={!window.isMac}>
          <RadioOption
            caption={i18n.t("activation_universal")}
            message={i18n.t("activation_universal_description")}
            checked={app.activation_mode() === ActivationMode.Universal}
            onClick={() => setActivationMode(ActivationMode.Universal)}
          />
          <RadioOption
            caption={i18n.t("activation_targeted")}
            message={i18n.t("activation_targeted_description")}
            checked={app.activation_mode() === ActivationMode.Targeted}
            onClick={() => setActivationMode(ActivationMode.Targeted)}
          />
        </Show>
        <Show when={window.isMac}>
          <RadioOption
            caption={i18n.t("activation_on_demand")}
            message={i18n.t("activation_on_demand_description")}
            disabled
            checked
          />
        </Show>
      </OptionSet>
    </div>
  );
};
