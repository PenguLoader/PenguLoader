use tauri::{
    plugin::{Builder, TauriPlugin},
    Runtime,
};

#[cfg(windows)]
use std::{fs, path::PathBuf, process::Command};

#[cfg(windows)]
use std::os::windows::process::CommandExt;

#[cfg(windows)]
const CREATE_NO_WINDOW: u32 = 0x08000000;

#[cfg(windows)]
const PREPARE_UPDATE: &str = r#"
param([string]$Destination)
$ErrorActionPreference = 'Stop'
$headers = @{
  'User-Agent' = 'PenguLoader-Updater/1.0'
  'Accept' = 'application/vnd.github+json'
  'X-GitHub-Api-Version' = '2022-11-28'
}
$releases = Invoke-RestMethod -Headers $headers -TimeoutSec 30 -Uri 'https://api.github.com/repos/PenguLoader/PenguLoader/releases?per_page=100'
$release = $releases | Where-Object {
  $_.prerelease -and -not $_.draft -and
  ($_.assets | Where-Object { $_.name.EndsWith('-main-ready-windows.zip') })
} | Select-Object -First 1
if (-not $release) { throw 'No Main-ready Windows release was found.' }
$asset = $release.assets | Where-Object { $_.name.EndsWith('-main-ready-windows.zip') } | Select-Object -First 1
$archive = Join-Path $Destination 'update.zip'
$unpacked = Join-Path $Destination 'unpacked'
New-Item -ItemType Directory -Force $unpacked | Out-Null
Invoke-WebRequest -Headers $headers -TimeoutSec 60 -Uri $asset.browser_download_url -OutFile $archive
Expand-Archive -LiteralPath $archive -DestinationPath $unpacked -Force
"#;

#[cfg(windows)]
const APPLY_UPDATE: &str = r#"
param([int]$TargetProcessId, [string]$Source, [string]$Destination, [string]$Executable)
$ErrorActionPreference = 'Stop'
Wait-Process -Id $TargetProcessId -ErrorAction SilentlyContinue
Get-ChildItem -LiteralPath $Source -Force | ForEach-Object {
  Copy-Item -LiteralPath $_.FullName -Destination $Destination -Recurse -Force
}
Start-Process -FilePath $Executable
"#;

#[cfg(windows)]
fn powershell() -> Command {
    let mut command = Command::new("powershell.exe");
    command.creation_flags(CREATE_NO_WINDOW);
    command
}

fn set_channel(content: &str, channel: &str) -> String {
    let mut found = false;
    let mut lines: Vec<String> = content
        .lines()
        .map(|line| {
            if line.trim_start().starts_with("UpdateChannel=") {
                found = true;
                format!("UpdateChannel={channel}")
            } else {
                line.to_string()
            }
        })
        .collect();

    if !found {
        lines.push(format!("UpdateChannel={channel}"));
    }

    lines.join("\n") + "\n"
}

#[cfg(windows)]
fn prepare_main_update(work_dir: &PathBuf) -> Result<PathBuf, String> {
    let unpacked = work_dir.join("unpacked");
    let prepare_script = work_dir.join("prepare.ps1");

    if work_dir.exists() {
        fs::remove_dir_all(work_dir).map_err(|error| error.to_string())?;
    }
    fs::create_dir_all(work_dir).map_err(|error| error.to_string())?;
    fs::write(&prepare_script, PREPARE_UPDATE).map_err(|error| error.to_string())?;

    let output = powershell()
        .args(["-NoProfile", "-ExecutionPolicy", "Bypass", "-File"])
        .arg(&prepare_script)
        .arg(&work_dir)
        .output()
        .map_err(|error| error.to_string())?;

    if !output.status.success() {
        return Err(String::from_utf8_lossy(&output.stderr).trim().to_string());
    }

    if !unpacked.join("Pengu Loader.exe").is_file() || !unpacked.join("version").is_file() {
        return Err("The Main-ready release is missing required files.".into());
    }

    Ok(unpacked)
}

#[cfg(windows)]
fn switch_to_main_windows<R: Runtime>(app: tauri::AppHandle<R>) -> Result<(), String> {
    let install_dir = crate::config::base_dir();
    let executable = install_dir.join("Pengu Loader.exe");
    let work_dir = std::env::temp_dir().join(format!("pengu_channel_{}", std::process::id()));
    let unpacked = prepare_main_update(&work_dir)?;
    let apply_script = work_dir.join("apply.ps1");

    let config_path = crate::config::config_path();
    let content = fs::read_to_string(&config_path).unwrap_or_default();
    fs::write(config_path, set_channel(&content, "stable")).map_err(|error| error.to_string())?;
    fs::write(&apply_script, APPLY_UPDATE).map_err(|error| error.to_string())?;

    powershell()
        .args([
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-WindowStyle",
            "Hidden",
            "-File",
        ])
        .arg(&apply_script)
        .arg(std::process::id().to_string())
        .arg(&unpacked)
        .arg(&install_dir)
        .arg(&executable)
        .spawn()
        .map_err(|error| error.to_string())?;

    app.exit(0);
    Ok(())
}

pub fn self_test() -> Result<(), String> {
    #[cfg(windows)]
    {
        let work_dir =
            std::env::temp_dir().join(format!("pengu_channel_self_test_{}", std::process::id()));
        let result = prepare_main_update(&work_dir);
        if result.is_ok() {
            let _ = fs::remove_dir_all(work_dir);
        }
        result.map(|_| ())
    }

    #[cfg(not(windows))]
    Ok(())
}

pub fn init<R: Runtime>() -> TauriPlugin<R> {
    #[tauri::command]
    fn switch_to_main<R: Runtime>(app: tauri::AppHandle<R>) -> Result<(), String> {
        #[cfg(windows)]
        return switch_to_main_windows(app);

        #[cfg(not(windows))]
        {
            let _ = app;
            Err("Switching to the WPF Main build is only available on Windows.".into())
        }
    }

    Builder::new("update_channel")
        .invoke_handler(tauri::generate_handler![switch_to_main])
        .build()
}

#[cfg(test)]
mod tests {
    use super::set_channel;

    #[test]
    fn replaces_or_adds_wpf_channel() {
        assert_eq!(
            set_channel("LeaguePath=C:\\Riot\nUpdateChannel=dev\n", "stable"),
            "LeaguePath=C:\\Riot\nUpdateChannel=stable\n"
        );
        assert_eq!(
            set_channel("[app]\nlanguage=en\n", "stable"),
            "[app]\nlanguage=en\nUpdateChannel=stable\n"
        );
    }
}
