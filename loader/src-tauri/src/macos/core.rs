use std::{path::PathBuf, process::Command};

fn get_insert_dylib() -> PathBuf {
    crate::config::base_dir().join("insert_dylib")
}

fn get_target_dylib_path(lol_dir: &PathBuf) -> PathBuf {
    lol_dir
        .join("League of Legends.app")
        .join("Contents")
        .join("Frameworks")
        .join("Chromium Embedded Framework.framework")
        .join("Libraries")
        .join("libEGL.dylib")
}

fn get_backup_dylib_path(lol_dir: &PathBuf) -> PathBuf {
    let mut p = get_target_dylib_path(lol_dir);
    p.set_extension("dylib.bak");
    p.into()
}

fn backup_module(lol_dir: &PathBuf) {
    let from = get_target_dylib_path(lol_dir);
    let to = get_backup_dylib_path(lol_dir);
    if from.exists() {
        _ = std::fs::copy(from, to);
    }
}

fn restore_module(lol_dir: &PathBuf) {
    let from = get_backup_dylib_path(lol_dir);
    let to = get_target_dylib_path(lol_dir);
    if from.exists() {
        _ = std::fs::copy(from.clone(), to);
        _ = std::fs::remove_file(from.clone());
    }
}

pub fn install_module(lol_dir: &PathBuf) -> bool {
    backup_module(lol_dir);

    if let Ok(status) = Command::new(get_insert_dylib())
        .arg("--all-yes")
        .arg("--inplace")
        .arg(crate::config::core_path().to_str().unwrap())
        .arg(get_target_dylib_path(lol_dir).to_str().unwrap())
        .status()
    {
        status.success()
    } else {
        false
    }

    // todo: integrate insert_dylib into Rust
}

pub fn uninstall_module(lol_dir: &PathBuf) {
    restore_module(lol_dir);
}
