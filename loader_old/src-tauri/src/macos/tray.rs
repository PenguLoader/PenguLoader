use tauri::{
    AppHandle, CustomMenuItem, Manager, Runtime, SystemTray, SystemTrayEvent, SystemTrayMenu,
    SystemTrayMenuItem,
};

pub fn create() -> SystemTray {
    let menu = SystemTrayMenu::new()
        .add_item(CustomMenuItem::new("".to_string(), "Pengu Loader").disabled())
        .add_item(CustomMenuItem::new("active".to_string(), "Active"))
        .add_native_item(SystemTrayMenuItem::Separator)
        .add_item(CustomMenuItem::new("hide".to_string(), "Show app"))
        .add_item(CustomMenuItem::new("quit".to_string(), "Quit"));

    SystemTray::new().with_menu(menu)
}

pub fn handle_event<R: tauri::Runtime>(app: &AppHandle<R>, evt: SystemTrayEvent) {
    if let SystemTrayEvent::MenuItemClick { id, .. } = evt {
        match id.as_str() {
            "active" => {
                super::cmd_set_active(app.app_handle(), !super::cmd_is_active());
            }
            "hide" => {
                let window = app.get_window("main").unwrap();
                window.show().unwrap();
            }
            "quit" => {
                app.exit(0);
            }
            _ => (),
        }
    }
}

pub fn set_active_check<R: Runtime>(app: AppHandle<R>, active: bool) {
    let menu_item = app.tray_handle().get_item("active");
    _ = menu_item.set_selected(active);
}
