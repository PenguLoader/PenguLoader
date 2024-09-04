use core::fmt;
use std::io::ErrorKind;
use tauri::{
    plugin::{Builder, TauriPlugin},
    Runtime,
};

mod mod_ifeo;
mod mod_symlink;
mod utils;

/// Enabling activation requires admin rights,
/// so we should encode the result to exitcode
/// when spawning a new process.

#[derive(Debug, Clone, Copy)]
enum ActivationStage {
    OpenIFEO = 1,
    CreateTarget,
    SetDebugger,
    DeleteDebugger,
    GetLeaguePath,
    CreateSymlink,
    DeleteSymlink,
    RunElevated,
}

impl fmt::Display for ActivationStage {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{:?}", self)
    }
}

type ActivationResult = Result<(), (ActivationStage, ErrorKind)>;

fn result_to_string(ret: ActivationResult) -> String {
    match ret {
        Ok(()) => format!(""),
        Err((stage, kind)) => {
            format!(
                "{} ({})",
                stage.to_string(),
                kind.to_string().replace(" ", "_")
            )
        }
    }
}

fn encode_result(ret: ActivationResult) -> i32 {
    match ret {
        Ok(()) => 0,
        Err((stage, kind)) => ((stage as i32) << 8) | (kind as i32),
    }
}

fn decode_result(code: i32) -> ActivationResult {
    if code == 0 {
        Ok(())
    } else {
        let stage =
            unsafe { std::mem::transmute::<i8, ActivationStage>(((code >> 8) & 0xff) as i8) };
        let kind = unsafe { std::mem::transmute::<i8, ErrorKind>((code & 0xff) as i8) };
        Err((stage, kind))
    }
}

fn do_activate(symlink: bool, active: bool) -> ActivationResult {
    if symlink {
        mod_symlink::do_activate(active)
    } else {
        mod_ifeo::do_activate(active)
    }
}

#[tauri::command]
fn core_is_activated(symlink: bool) -> bool {
    if symlink {
        mod_symlink::is_activated()
    } else {
        mod_ifeo::is_activated()
    }
}

/// Do activate command.
#[tauri::command]
fn core_do_activate(symlink: bool, active: bool) -> String {
    // elevated process
    // or disabling symlink
    // or symlink with devmode
    if utils::is_admin() || (symlink && !active) || (symlink && utils::is_developer()) {
        return result_to_string(do_activate(symlink, active));
    }

    // must spawn a thread to avoid UAC freezing
    let result = std::thread::spawn(move || -> ActivationResult {
        match runas::Command::new(std::env::current_exe().unwrap())
            .arg(if active { "--install" } else { "--uninstall" })
            .arg(if symlink { "--symlink" } else { "" })
            .show(false)
            .status()
        {
            Ok(status) => {
                let code = status.code().unwrap_or(-1);
                if code < 0 {
                    Err((ActivationStage::RunElevated, ErrorKind::PermissionDenied))
                } else {
                    decode_result(code)
                }
            }
            Err(err) => Err((ActivationStage::RunElevated, err.kind())),
        }
    })
    .join()
    .unwrap();

    result_to_string(result)
}

fn plugin<R: Runtime>() -> TauriPlugin<R> {
    Builder::new("windows")
        .invoke_handler(tauri::generate_handler![
            core_is_activated,
            core_do_activate,
        ])
        .build()
}

impl<R: Runtime> super::CustomBuild for tauri::Builder<R> {
    fn setup_platform(self) -> Self {
        self.setup(|app| {
            let window = super::build_window(app);
            utils::enable_shadow(window.hwnd().unwrap().0);
            Ok(())
        })
        .plugin(plugin())
    }
}

pub fn do_entry() {
    let args: Vec<String> = std::env::args().collect();
    let mut avtive_value: Option<bool> = None;

    if args.len() > 1 {
        if args[1] == "--install" {
            avtive_value = Some(true);
        } else if args[1] == "--uninstall" {
            avtive_value = Some(false);
        }
    }

    if let Some(active) = avtive_value {
        let symlink = args.len() > 2 && args[2] == "--symlink";
        let result = do_activate(symlink, active);
        std::process::exit(encode_result(result))
    }

    if !utils::is_webview2_installed() {
        #[link(name = "user32")]
        extern "system" {
            fn MessageBoxA(_: isize, message: *const u8, caption: *const u8, flags: u32) -> i32;
        }
        unsafe {
            MessageBoxA(
                0,
                "WebView2 is not installed on your system.\n\
                Please install WebView2 to run the app.\0"
                    .as_ptr(),
                "Pengu Loader\0".as_ptr(),
                0x30,
            );
        }
        std::process::exit(1)
    }
}
