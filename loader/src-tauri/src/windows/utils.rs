use std::io::Error;
use winreg::{
    enums::{HKEY_LOCAL_MACHINE, KEY_READ},
    RegKey,
};

/// Check if Developer Mode is active.
pub fn is_developer() -> bool {
    let hklm = RegKey::predef(HKEY_LOCAL_MACHINE);
    const REG_PATH: &str = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\AppModelUnlock";

    if let Ok(key) = hklm.open_subkey_with_flags(REG_PATH, KEY_READ) {
        if let Ok(value) = key.get_value("AllowDevelopmentWithoutDevLicense") as Result<u32, Error>
        {
            return value == 1;
        }
    }

    false
}

/// Check if the running process is admin.
pub fn is_admin() -> bool {
    is_elevated::is_elevated()
}

/// Fix borderless window shadow.
/// Don't use windows_sys because dependencies hell.
pub fn enable_shadow(hwnd: isize) {
    use libc::c_void;

    #[repr(C)]
    struct MARGINS {
        left: i32,
        right: i32,
        top: i32,
        bottom: i32,
    }

    #[link(name = "dwmapi")]
    extern "C" {
        fn DwmExtendFrameIntoClientArea(hwnd: isize, pMarInset: *const MARGINS) -> i32;
        fn DwmSetWindowAttribute(
            hwnd: isize,
            dwAttribute: i32,
            pvAttribute: *const c_void,
            cbAttribute: u32,
        );
    }

    unsafe {
        DwmSetWindowAttribute(hwnd, 2, &2 as *const _ as _, 4);
        let margins = MARGINS {
            left: 0,
            right: 0,
            top: 1,
            bottom: 0,
        };
        DwmExtendFrameIntoClientArea(hwnd, &margins);
    }
}
