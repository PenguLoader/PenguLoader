using System;

namespace LeagueLoader
{
    class Language
    {
        public string LeaguePath;
        public string Msg_SelectLeaguePath;
        public string InsecureOptions;
        public string OpenPlugins;
        public string OpenDevTools;
        public string RestartClient;
        public string Install;
        public string Uninstall;

        public string WarningICE;
        public string WarningDWS;
        public string WarningRDP;

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
            InsecureOptions = "Insecure options",
            OpenPlugins = "Open plugins folder",
            OpenDevTools = "Open DevTools",
            RestartClient = "Restart Client",
            Install = "INSTALL",
            Uninstall = "UNINSTALL",

            WarningICE = "Warning about insecure option!\n" +
                "\n" +
                "Ignoring certificate errors helps you to ignore all error/invalid SSL certificates. " +
                "That's against Riot's Privacy Policy, so you might get banned.\n" +
                "\n" +
                "Are you sure want to continue?",

            WarningDWS = "Warning about insecure option!\n" +
                "\n" +
                "Disabling web security helps you to bypass CORS when making request with fetch() and XHR (in League Client). " +
                "That's against Riot's Privacy Policy, so you might get banned.\n" +
                "\n" +
                "Are you sure want to continue?",

            WarningRDP = "Warning about insecure option!\n" +
                "\n" +
                "Using remote debuggin port helps you to control League Client's DevTools remotely through a specified port. " +
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
            InsecureOptions = "Các tùy chọn không an toàn",
            OpenPlugins = "Mở thư mục plugins",
            OpenDevTools = "Mở DevTools",
            RestartClient = "Khởi động lại Client",
            Install = "CÀI ĐẶT",
            Uninstall = "GỠ CÀI ĐẶT",

            WarningICE = "Cảnh báo tùy chọn không an toàn!\n" +
                "\n" +
                "Bật tùy chọn này giúp bỏ qua tất cả các chứng chỉ SSL lỗi/không hợp lệ. " +
                "Điều này vi phạm chính sách quyền riêng tư của Riot và có thể bị khóa tài khoản.\n" +
                "\n" +
                "Bạn có muốn tiếp tục không?",

            WarningDWS = "Cảnh báo tùy chọn không an toàn!\n" +
                "\n" +
                "Bật tùy chọn này giúp bypass CORS khi thực hiện request với fetch() và XHR (trong League Client). " +
                "Điều này vi phạm chính sách quyền riêng tư của Riot và có thể bị khóa tài khoản.\n" +
                "\n" +
                "Bạn có muốn tiếp tục không?",

            WarningRDP = "Cảnh báo tùy chọn không an toàn!\n" +
                "\n" +
                "Bật tùy chọn này giúp diêu khiển DevTools của League Client từ xa thông qua một port xác định. " +
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