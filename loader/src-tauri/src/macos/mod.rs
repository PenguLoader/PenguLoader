#![cfg(target_os = "macos")]

use tauri::{
    plugin::{Builder, TauriPlugin},
    Runtime,
};

#[tauri::command]
fn initialize() {}

pub fn init<R: Runtime>() -> TauriPlugin<R> {
    Builder::new("macos")
        .invoke_handler(tauri::generate_handler![initialize])
        .build()
}
