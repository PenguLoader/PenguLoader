use std::os::unix::fs::PermissionsExt;
use std::{path::PathBuf, process::Command};

use crate::dprintln;

fn get_insert_dylib() -> PathBuf {
    crate::config::base_dir().join("insert_dylib")
}

fn ensure_exec_perm(path: &PathBuf) {
    if path.exists() {
        let metadata = std::fs::metadata(path).unwrap();
        let permissions = metadata.permissions();
        let current_mode = permissions.mode();

        if current_mode & 0o111 == 0 {
            let mut new_permissions = permissions;
            new_permissions.set_mode(current_mode | 0o111);
            std::fs::set_permissions(path, new_permissions).unwrap();

            dprintln!("added exec perm for {:?}", path);
        }
    }
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

    let insert_dylib_path = get_insert_dylib();

    if insert_dylib_path.exists() {
        dprintln!("use external insert_dylib");
        ensure_exec_perm(&insert_dylib_path);

        if let Ok(status) = Command::new(insert_dylib_path)
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
    } else {
        dprintln!("use built-in insert_dylib");

        let core_path = crate::config::core_path();
        let target_path = get_target_dylib_path(lol_dir);

        unsafe {
            super::dylib::insert(
                core_path.to_str().unwrap(),
                target_path.to_str().unwrap(),
                false,
                true,
            )
        }
    }
}

pub fn uninstall_module(lol_dir: &PathBuf) {
    restore_module(lol_dir);
}
