use std::path::PathBuf;

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

    let core_path = crate::config::core_path();
    let target_path = get_target_dylib_path(lol_dir);

    unsafe {
        super::dylib::insert(
            core_path.to_str().unwrap(),
            target_path.to_str().unwrap(),
            false,
        )
    }
}

pub fn uninstall_module(lol_dir: &PathBuf) {
    restore_module(lol_dir);
}
