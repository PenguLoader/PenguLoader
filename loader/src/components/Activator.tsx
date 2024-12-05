import { Component, createSignal, onMount } from "solid-js";
import { Dynamic } from "solid-js/web";
import { CoreModule } from "../lib/core-module";
import { dialog, event } from "@tauri-apps/api";
import { BoltIcon, PowerIcon } from "./Icons";
import { useI18n } from "~/lib/i18n";

export const Activator: Component = () => {
  const i18n = useI18n();
  const [loading, setLoading] = createSignal(true);
  const [active, setActive] = createSignal(false);

  const activate = async () => {
    if (!loading()) {
      setLoading(true);

      try {
        if (!(await CoreModule.checkLeagueDir())) {
          await dialog.message(i18n.t("invalid_lol_folder"), {
            type: "warning",
          });
          return;
        }

        if (!(await CoreModule.exists())) {
          await dialog.message(i18n.t("core_not_found"), { type: "warning" });
          return;
        }

        const nextActive = !active();
        const { activated, error } = await CoreModule.doActivate(nextActive);

        if (error) {
          await dialog.message(`${i18n.t("activation_error")}\n${error}`, {
            type: "warning",
          });
        } else if (activated === nextActive) {
          setActive(activated);
        }
      } finally {
        setLoading(false);
      }
    }
  };

  onMount(async () => {
    setActive(await CoreModule.isActivated());
    setLoading(false);

    if (window.isMac) {
      event.listen("active-status", (e) => {
        setActive(Boolean(e.payload));
      });
    }
  });

  return (
    <div class="fixed bottom-6 right-0 z-10 translate-x-28 hover:translate-x-0 transition-transform">
      <div
        class="flex items-center justify-between pl-3 shadow-lg w-44 h-14 rounded-l-full border border-neutral-700/30 border-r-0
        cursor-pointer aria-disabled::cursor-not-allowed group bg-card aria-checked:bg-primary
        hover:shadow-xl transition-colors ease-out duration-300"
        aria-disabled={loading()}
        aria-checked={active()}
        onClick={activate}
      >
        <div
          class="flex items-center justify-center size-8 text-primary rounded-full group-hover:bg-primary
          aria-checked:bg-muted group-hover:text-accent group-hover:aria-checked:bg-muted group-hover:aria-checked:text-primary"
          aria-checked={active()}
        >
          <span class="group-hover:animate-pulse">
            <Dynamic
              component={active() ? BoltIcon : PowerIcon}
              thickness={2.5}
            />
          </span>
        </div>
        <div
          class="flex-1 px-6 text-lg text-center font-semibold text-primary aria-checked:text-muted"
          aria-checked={active()}
        >
          {active() ? i18n.t("ready") : i18n.t("activate")}
        </div>
      </div>
    </div>
  );
};
