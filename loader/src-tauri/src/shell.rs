use std::process::Command;
use tauri::{
    plugin::{Builder, TauriPlugin},
    Runtime,
};

#[tauri::command]
fn expand_folder(path: &str) {
    #[cfg(windows)]
    Command::new("explorer")
        .args(["/expand,", path])
        .spawn()
        .unwrap();

    #[cfg(target_os = "macos")]
    Command::new("open").arg(path).spawn().unwrap();
}

#[tauri::command]
fn reveal_file(path: &str) {
    #[cfg(windows)]
    Command::new("explorer")
        .args(["/select,", path])
        .spawn()
        .unwrap();

    #[cfg(target_os = "macos")]
    Command::new("open").args(["-R", path]).spawn().unwrap();
}

pub fn init<R: Runtime>() -> TauriPlugin<R> {
    Builder::new("shell")
        .invoke_handler(tauri::generate_handler![expand_folder, reveal_file,])
        .build()
}
