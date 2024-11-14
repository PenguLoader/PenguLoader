use std::{
    fs::File,
    io::{BufRead, BufReader},
    path::PathBuf,
};
use tauri::{
    plugin::{Builder, TauriPlugin},
    Runtime,
};

/// Base dir is the exe dir.
/// In dev, it should be the bin folder in root project.
pub fn base_dir() -> PathBuf {
    let path = std::env::current_exe().unwrap();
    let dir = path.parent().unwrap();

    if cfg!(debug_assertions) {
        return dir
            .parent()
            .unwrap()
            .parent()
            .unwrap()
            .parent()
            .unwrap()
            .parent()
            .unwrap()
            .join("bin");
    }

    dir.into()
}

/// Get core module path.
pub fn core_path() -> PathBuf {
    let mut path = base_dir().join("core");
    #[cfg(windows)]
    path.set_extension("dll");
    #[cfg(target_os = "macos")]
    path.set_extension("dylib");
    path
}

/// Get config path.
pub fn config_path() -> PathBuf {
    let dir = base_dir();
    dir.join("config")
}

/// Get League dir from the config.
#[allow(dead_code)]
pub fn league_dir() -> Option<PathBuf> {
    if let Ok(file) = File::open(config_path()) {
        let reader = BufReader::new(file);
        let mut league_path = String::new();
        for line in reader.lines() {
            if let Ok(line) = line {
                if line.trim_start().starts_with("league_dir") {
                    if let Some(index) = line.find('=') {
                        if index > 0 {
                            league_path = line[(index + 1)..].trim().to_string();
                            break;
                        }
                    }
                    break;
                }
            }
        }
        if !league_path.is_empty() {
            Some(PathBuf::from(league_path))
        } else {
            None
        }
    } else {
        None
    }
}

pub fn init<R: Runtime>() -> TauriPlugin<R> {
    #[tauri::command]
    fn get_base_dir() -> String {
        base_dir().display().to_string()
    }

    #[tauri::command]
    fn get_config_path() -> String {
        config_path().display().to_string()
    }

    #[tauri::command]
    fn core_exists() -> bool {
        core_path().exists()
    }

    Builder::new("config")
        .invoke_handler(tauri::generate_handler![
            get_base_dir,
            get_config_path,
            core_exists
        ])
        .build()
}
