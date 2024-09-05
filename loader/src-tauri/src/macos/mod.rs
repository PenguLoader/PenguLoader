use std::fs;
use std::path::PathBuf;
use std::sync::atomic::{AtomicBool, Ordering};
use tauri::{
    plugin::{Builder, TauriPlugin},
    App, AppHandle, Manager, Runtime,
};

mod core;
mod dylib;
mod socket;
mod startup;
mod tray;
mod utils;

static ACTIVE: AtomicBool = AtomicBool::new(false);

fn active_file_path() -> PathBuf {
    super::config::base_dir().join("active")
}

fn set_active(active: bool) {
    ACTIVE.store(active, Ordering::SeqCst);
    _ = fs::write(active_file_path(), if active { "1" } else { "0" });
}

#[tauri::command]
pub fn cmd_is_active() -> bool {
    ACTIVE.load(Ordering::SeqCst)
}

#[tauri::command]
pub fn cmd_set_active<R: Runtime>(app: AppHandle<R>, active: bool) {
    set_active(active);
    _ = app.emit_all("active-status", active);
    tray::set_active_check(app, active);
}

fn initialize<R: Runtime>(app: &App<R>) {
    let window = super::build_window(app);
    utils::hide_title_bar(window.ns_window().unwrap());
    utils::hide_traffic_lights(window.ns_window().unwrap());

    utils::set_app_delegate_hook(move || {
        window.center().unwrap();
        window.show().unwrap();
    });

    cmd_set_active(
        app.app_handle(),
        match fs::read_to_string(active_file_path()) {
            Ok(value) => value == "1",
            _ => false,
        },
    );

    socket::run_daemon();
}

fn plugin<R: Runtime>() -> TauriPlugin<R> {
    Builder::new("macos")
        .invoke_handler(tauri::generate_handler![cmd_is_active, cmd_set_active,])
        .build()
}

impl<R: Runtime> super::CustomBuild for tauri::Builder<R> {
    fn setup_platform(self) -> Self {
        self.system_tray(tray::create())
            .on_system_tray_event(tray::handle_event)
            .setup(|app| {
                initialize(app);
                Ok(())
            })
            .plugin(plugin())
            .plugin(startup::init())
    }
}
