using System;

namespace LeagueLoader
{
    internal class Language
    {
        public string LeaguePath;
        public string Msg_SelectLeaguePath;
        public string RemoteDebugger;
        public string OpenPlugins;
        public string OpenDevTools;
        public string RestartClient;
        public string Install;
        public string Uninstall;

        public string EnableWithPort;
        public string WarningRemoteDebugger;

        public string Msg_InvalidSelectedPath;
        public string Msg_NotActivated;
        public string Msg_LeagueNotOpened;

        public string Msg_ModuleInstalled;
        public string Msg_ModuleInstalled_Running;
        public string Msg_ModuleUninstall;
        public string Msg_ModuleUninstalled;
        public string Msg_ModuleUninstalled_Loaded;

        public static Language English = new Language
        {
            LeaguePath = "League Client path",
            Msg_SelectLeaguePath = "Select Riot Games, League of Legends or LeagueClient folder.",
            RemoteDebugger = "Remote Debugger",
            OpenPlugins = "Open plugins folder",
            OpenDevTools = "Open DevTools",
            RestartClient = "Restart Client",
            Install = "INSTALL",
            Uninstall = "UNINSTALL",

            EnableWithPort = "Enable with port",
            WarningRemoteDebugger = "Warning!\n" +
                "\n" +
                "Using remote debugger helps you to control League Client's DevTools remotely through a specified port. " +
                "This can be dangerous if you are using public network.\n" +
                "\n" +
                "Are you sure want to continue?",

            Msg_InvalidSelectedPath = "Your selected path is invalid.",
            Msg_NotActivated = "League Loader is not activated.",
            Msg_LeagueNotOpened = "League Client is not opened.",

            Msg_ModuleInstalled = "League Loader module has been added to League Client.\n" +
                "Please launch League Client to activate it.",
            Msg_ModuleInstalled_Running = "League Loader module has been added to League Client.\n" +
                "Do you want to restart League Client to activate it?",
            Msg_ModuleUninstall = "Do you want to remove League Loader module from League Client?",
            Msg_ModuleUninstalled = "League Loader module has been removed from League Client.",
            Msg_ModuleUninstalled_Loaded = "League Loader module has been removed from League Client.\n" +
                "Do you want to restart League Client to deactivate it?",
        };

        public static Language Vietnamese = new Language
        {
            LeaguePath = "Đường dẫn Client LMHT",
            Msg_SelectLeaguePath = "Vui lòng chọn đường dẫn đến thư mục Riot Games, League of Legends hoặc LeagueClient.",
            RemoteDebugger = "Gỡ lỗi từ xa",
            OpenPlugins = "Mở thư mục plugins",
            OpenDevTools = "Mở DevTools",
            RestartClient = "Khởi động lại Client",
            Install = "CÀI ĐẶT",
            Uninstall = "GỠ CÀI ĐẶT",

            EnableWithPort = "Dùng với port",
            WarningRemoteDebugger = "Cảnh báo!\n" +
                "\n" +
                "Bật tùy chọn này giúp điêu khiển DevTools của League Client từ xa thông qua một port xác định. " +
                "Điều này có thể gây nguy hiểm nếu bạn sử dụng mạng công cộng.\n" +
                "\n" +
                "Bạn có muốn tiếp tục không?",

            Msg_InvalidSelectedPath = "Đường dẫn mà bạn chọn không hợp lệ.",
            Msg_NotActivated = "League Loader chưa được kích hoạt.",
            Msg_LeagueNotOpened = "League Client chưa được mở.",

            Msg_ModuleInstalled = "Module League Loader đã được thêm vào League Client.\n" +
                "Vui lòng chạy League Client để kích hoạt nó.",
            Msg_ModuleInstalled_Running = "Module League Loader đã được thêm vào League Client.\n" +
                "Bạn có muốn khởi động lại League Client để kích hoạt nó?",
            Msg_ModuleUninstall = "Bạn có muốn gỡ module League Loader khỏi League Client?",
            Msg_ModuleUninstalled = "Module League Loader đã được gỡ khỏi League Client.",
            Msg_ModuleUninstalled_Loaded = "Module League Loader đã được gỡ khỏi League Client.\n" +
                "Bạn có muốn khởi động lại League Client để hủy kích hoạt nó?",
        };
    }
}