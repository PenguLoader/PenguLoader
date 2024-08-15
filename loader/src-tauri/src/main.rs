// Prevents additional console window on Windows in release, DO NOT REMOVE!!
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use named_lock::{Error, NamedLock};
use std::env;
use tauri::Manager;

mod config;
mod shell;

#[cfg(windows)]
mod windows;

#[cfg(target_os = "macos")]
mod macos;

fn main() -> Result<(), Error> {
    #[cfg(windows)]
    windows::do_entry();

    // todo: add webview2 check on windows

    let lock = NamedLock::create("989d2110-46da-4c8d-84c1-c4a42e43c424")?;
    let _guard = lock.try_lock()?;

    match tauri::Builder::default() {
        builder => {
            #[cfg(windows)]
            {
                builder.plugin(windows::init()).setup(|app| {
                    let window = app.get_window("main").unwrap();
                    windows::enable_shadow(window.hwnd().unwrap().0);
                    Ok(())
                })
            }
            #[cfg(target_os = "macos")]
            {
                builder.plugin(macos::init())
            }
        }
    }
    .plugin(config::init())
    .plugin(shell::init())
    .run(tauri::generate_context!())
    .expect("error while running tauri application");

    Ok(())
}
