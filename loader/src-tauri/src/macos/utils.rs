use base64::{prelude::BASE64_STANDARD, Engine};
use objc::{
    class,
    declare::ClassDecl,
    msg_send,
    runtime::{Object, Sel},
    sel, sel_impl,
};
use std::net::{IpAddr, Ipv4Addr, SocketAddr};
use sysinfo::{ProcessRefreshKind, RefreshKind, System, UpdateKind};

/// Get the running RiotClient's address & authorization from command line.
pub fn get_riotclient_info() -> Option<(SocketAddr, String)> {
    let refresh_kind = ProcessRefreshKind::new()
        .with_exe(UpdateKind::OnlyIfNotSet)
        .with_cmd(UpdateKind::OnlyIfNotSet);

    let system = System::new_with_specifics(RefreshKind::new().with_processes(refresh_kind));

    if let Some(process) = system
        .processes()
        .values()
        .find(|process| process.name() == "Riot Client")
    {
        let cmd = process.cmd().iter().filter_map(|os_str| os_str.to_str());

        let mut auth = None;
        let mut port = None;

        for s in cmd {
            if auth.is_none() {
                auth = s.strip_prefix("--remoting-auth-token=");
            }

            if port.is_none() {
                port = s.strip_prefix("--app-port=");
            }

            if auth.is_some() && port.is_some() {
                let addr = SocketAddr::new(
                    IpAddr::V4(Ipv4Addr::LOCALHOST),
                    port.unwrap().parse::<u16>().unwrap(),
                );
                let token = BASE64_STANDARD.encode(format!("riot:{}", auth.unwrap()));

                return Some((addr, token));
            }
        }
    }

    None
}

/// Hide the traffic light controls from window.
pub fn hide_traffic_lights(window: *mut std::ffi::c_void) {
    unsafe {
        let window: *mut Object = std::mem::transmute(window);

        let min_button: *mut Object = msg_send![window, standardWindowButton:0];
        let close_button: *mut Object = msg_send![window, standardWindowButton:1];
        let max_button: *mut Object = msg_send![window, standardWindowButton:2];

        let _: () = msg_send![min_button, setHidden: true];
        let _: () = msg_send![close_button, setHidden: true];
        let _: () = msg_send![max_button, setHidden: true];
    }
}

/// Hide window's title bar.
pub fn hide_title_bar(window: *mut std::ffi::c_void) {
    unsafe {
        let window: *mut Object = std::mem::transmute(window);

        let _: () = msg_send![window, setTitlebarAppearsTransparent: true];
        let _: () = msg_send![window, setTitleVisibility: 1];
    }
}

/// Add event to existing NSApplication.
/// * `handle_reopen`: called when user clicked on the dock icon.
pub fn set_app_delegate_hook<F>(handle_reopen: F)
where
    F: Fn() + Send + 'static,
{
    static mut CALLBACK: Option<Box<dyn Fn() + Send + 'static>> = None;

    extern "C" fn callback_reopen(_: &Object, _: Sel, _: *mut Object, visible: bool) -> bool {
        if !visible {
            unsafe {
                if let Some(ref callback) = CALLBACK {
                    callback();
                }
            }
        }
        true
    }

    unsafe {
        let app: *mut Object = msg_send![class!(NSApplication), sharedApplication];
        let old_delegate: *mut Object = msg_send![app, delegate];

        // extend the default TAO NSApplicationDelegate
        if let Some(mut decl) = ClassDecl::new("AppDelegate", (*old_delegate).class()) {
            CALLBACK = Some(Box::new(handle_reopen));

            decl.add_method(
                sel!(applicationShouldHandleReopen:hasVisibleWindows:),
                callback_reopen as extern "C" fn(&Object, Sel, *mut Object, bool) -> bool,
            );

            // override it
            let new_delegate: *mut Object = msg_send![decl.register(), new];
            let _: () = msg_send![app, setDelegate: new_delegate];
        }
    }
}
