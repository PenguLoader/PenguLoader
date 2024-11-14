use std::io::Error;
use winreg::{enums::*, RegKey};

use super::{ActivationResult, ActivationStage};
use crate::config;

const IFEO_PATH: &str =
    "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options";
const TARGET_EXE: &str = "LeagueClientUx.exe";

/// HKLM
/// \ IFEO
///   \ LeagueClientUx.exe
///     + Debugger = rundll32 "path\to\core.dll", entry

fn extract_path(value: &String) -> Option<String> {
    if let Some(start) = value.find('"') {
        if let Some(end) = value[start + 1..].find('"') {
            return Some(value[start + 1..start + 1 + end].to_string());
        }
    }
    None
}

fn normalize_path(path: &String) -> String {
    path.to_lowercase().replace('/', "\\")
}

/// Debugger value for IFEO target subkey.
fn get_debugger_value() -> String {
    const DLL_ENTRY: &str = "#6000";
    let dll_path = config::core_path();
    format!("rundll32 \"{}\", {}", dll_path.display(), DLL_ENTRY)
}

/// Check if the core module is activated.
pub fn is_activated() -> bool {
    let hklm = RegKey::predef(HKEY_LOCAL_MACHINE);
    let path = [IFEO_PATH, TARGET_EXE].join("\\");

    // open lcux target
    if let Ok(key) = hklm.open_subkey_with_flags(path, KEY_READ) {
        // read debugger value
        if let Ok(value) = key.get_value("Debugger") as Result<String, Error> {
            // the debuger value must use rundll32
            if value.to_lowercase().starts_with("rundll32") {
                // extract the path from debugger value
                if let Some(debugger_path) = extract_path(&value) {
                    // compare two paths
                    return normalize_path(&debugger_path)
                        == normalize_path(
                            &config::core_path().into_os_string().into_string().unwrap(),
                        );
                }
            }
        }
    }

    false
}

/// Perform IFEO activation.
/// Requires admin rights.
pub fn do_activate(active: bool) -> ActivationResult {
    let hklm = RegKey::predef(HKEY_LOCAL_MACHINE);
    let ifeo = match hklm.open_subkey_with_flags(IFEO_PATH, KEY_CREATE_SUB_KEY) {
        Ok(sk) => sk,
        Err(err) => return Err((ActivationStage::OpenIFEO, err.kind())),
    };

    let lcux = match ifeo.create_subkey_with_flags(TARGET_EXE, KEY_SET_VALUE) {
        Ok(subkey) => subkey.0,
        Err(err) => return Err((ActivationStage::CreateTarget, err.kind())),
    };

    if active {
        let value = get_debugger_value();
        match lcux.set_value("Debugger", &value) {
            Ok(()) => Ok(()),
            Err(err) => Err((ActivationStage::SetDebugger, err.kind())),
        }
    } else {
        match lcux.delete_value("Debugger") {
            Ok(()) => Ok(()),
            Err(err) => Err((ActivationStage::DeleteDebugger, err.kind())),
        }
    }
}
