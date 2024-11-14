use auto_launch::{AutoLaunch, AutoLaunchBuilder};
use tauri::{
    plugin::{Builder, TauriPlugin},
    Manager, Runtime, State,
};

struct AutoLaunchManager(AutoLaunch);

impl AutoLaunchManager {
    pub fn set_enable(&self, enable: bool) -> Result<(), String> {
        if enable {
            self.0.enable()
        } else {
            self.0.disable()
        }
        .map_err(|e| e.to_string())
    }

    pub fn is_enabled(&self) -> Result<bool, String> {
        self.0.is_enabled().map_err(|e| e.to_string())
    }
}

#[tauri::command]
async fn is_enabled(manager: State<'_, AutoLaunchManager>) -> Result<bool, String> {
    manager.is_enabled()
}

#[tauri::command]
async fn set_enable(manager: State<'_, AutoLaunchManager>, enable: bool) -> Result<(), String> {
    manager.set_enable(enable)
}

pub fn init<R: Runtime>() -> TauriPlugin<R> {
    Builder::new("startup")
        .invoke_handler(tauri::generate_handler![is_enabled, set_enable])
        .setup(|app| {
            let mut builder = AutoLaunchBuilder::new();
            builder.set_app_name(&app.package_info().name);
            builder.set_args(&["--silent"]);
            builder.set_use_launch_agent(false);

            let exe_path = std::env::current_exe().unwrap();
            builder.set_app_path(&exe_path.canonicalize()?.display().to_string());

            app.manage(AutoLaunchManager(
                builder.build().map_err(|e| e.to_string())?,
            ));

            Ok(())
        })
        .build()
}
