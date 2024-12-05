import { Component } from "solid-js";
import { useConfig } from "~/lib/config";
import { CheckOption, OptionSet } from "./templates";
import { useI18n } from "~/lib/i18n";

export const TabClient: Component = () => {
  const i18n = useI18n();
  const { client } = useConfig();

  return (
    <div class="space-y-4">
      <p class="text-sm text-neutral-400">
        {i18n.t("client_settings_observation")}
      </p>

      <OptionSet name={i18n.t("hot_keys")}>
        <CheckOption
          caption={i18n.t("enable_hot_keys")}
          message={i18n.t("enable_hot_keys_description")}
          checked={client.use_hotkeys()}
          onChange={client.use_hotkeys}
        />
        <div
          class="space-y-2 ml-8 aria-disabled:opacity-50"
          aria-disabled={!client.use_hotkeys()}
        >
          <div class="flex items-center space-x-2">
            <kbd class="px-2 py-0.5 rounded-sm text-xs bg-neutral-500/30">
              Ctrl Shift R
            </kbd>
            <p class="text-sm text-neutral-400">{i18n.t("reload_client")}</p>
          </div>
          <div class="flex items-center space-x-2">
            <kbd class="px-2 py-0.5 rounded-sm text-xs bg-neutral-500/30">
              Ctrl Shift Enter
            </kbd>
            <p class="text-sm text-neutral-400">
              {i18n.t("restart_interface")}
            </p>
          </div>
          <div
            class="flex items-center space-x-2 aria-disabled:line-through"
            aria-disabled={!client.use_devtools()}
          >
            <kbd class="px-2 py-0.5 rounded-sm text-xs bg-neutral-500/30">
              Ctrl Shift I
            </kbd>
            <span>/</span>
            <kbd class="px-2 py-0.5 rounded-sm text-xs bg-neutral-500/30">
              F12
            </kbd>
            <p class="text-sm text-neutral-400">{i18n.t("open_dev_tools")}</p>
          </div>
        </div>
      </OptionSet>

      <OptionSet name={i18n.t("tweaks")}>
        <CheckOption
          caption={i18n.t("optimize_client")}
          message={i18n.t("optimize_client_description")}
          checked={client.optimized_client()}
          onChange={client.optimized_client}
        />
        <CheckOption
          caption={i18n.t("super_potato_mode")}
          message={i18n.t("super_potato_mode_description")}
          checked={client.super_potato()}
          onChange={client.super_potato}
        />
        <CheckOption
          caption={i18n.t("silent_mode")}
          message={i18n.t("silent_mode_description")}
          checked={client.silent_mode()}
          onChange={client.silent_mode}
        />
      </OptionSet>

      <OptionSet name={i18n.t("developer")}>
        <CheckOption
          caption={i18n.t("developer_tools")}
          message={i18n.t("developer_tools_description")}
          checked={client.use_devtools()}
          onChange={client.use_devtools}
        />
        <CheckOption
          caption={i18n.t("secure_mode")}
          message={i18n.t("secure_mode_description")}
          checked={client.insecure_mode()}
          onChange={client.insecure_mode}
        />
        <CheckOption
          caption={i18n.t("riot_client_api")}
          message={i18n.t("riot_client_api_description")}
          checked={client.use_riotclient()}
          onChange={client.use_riotclient}
        />
        <CheckOption
          caption={i18n.t("allow_proxy")}
          message={i18n.t("allow_proxy_description")}
          checked={client.use_proxy()}
          onChange={client.use_proxy}
        />
      </OptionSet>
    </div>
  );
};
