use std::fs::read_link;
use std::io::ErrorKind;
use std::os::windows::fs::symlink_file;
use std::path::PathBuf;

use super::{ActivationResult, ActivationStage};
use crate::config;

/// The symlink filename in LoL folder.
/// It must not be loaded by LeagueClient due to Vanguard.
/// The supported are d3d9.dll, dwrite.dll, version.dll.
const TARGET_FNAME: &str = "version.dll";

/// Check if a file path is symlink or not.
fn is_symlink(path: &PathBuf) -> bool {
    if let Ok(metadata) = std::fs::symlink_metadata(path) {
        metadata.file_type().is_symlink()
    } else {
        false
    }
}

/// The symlink path to be installed in LoL folder.
fn get_symlink_path() -> Option<PathBuf> {
    match config::league_dir() {
        Some(dir) => Some(dir.join(TARGET_FNAME).to_path_buf()),
        _ => None,
    }
}

/// Check if the symlink is installed or not.
pub fn is_activated() -> bool {
    if let Some(link_path) = get_symlink_path() {
        if link_path.exists() && is_symlink(&link_path) {
            if let Ok(target_path) = read_link(link_path) {
                // compare target path with core path
                return target_path == config::core_path();
            }
        }
    }
    false
}

/// Perform symlink activation.
/// Requires admin rights or Developer Mode when `active` is true.
pub fn do_activate(active: bool) -> ActivationResult {
    if let Some(link_path) = get_symlink_path() {
        if active {
            let orig_path = crate::config::core_path();
            match symlink_file(orig_path, link_path) {
                Ok(()) => Ok(()),
                Err(err) => Err((ActivationStage::CreateSymlink, err.kind())),
            }
        } else {
            if link_path.exists() {
                match std::fs::remove_file(link_path) {
                    Ok(()) => Ok(()),
                    Err(err) => Err((ActivationStage::DeleteSymlink, err.kind())),
                }
            } else {
                Ok(())
            }
        }
    } else {
        Err((ActivationStage::GetLeaguePath, ErrorKind::NotFound))
    }
}
