#include "internal.h"

#include <wininet.h>
#pragma comment(lib, "wininet.lib")

using namespace league_loader;

// BROWSER PROCESS ONLY.

extern UINT REMOTE_DEBUGGING_PORT;
extern cef_browser_t *CLIENT_BROWSER;
static std::string REMOTE_DEVTOOLS_URL;

void FetchRemoteDevToolsUrl()
{
    if (REMOTE_DEBUGGING_PORT == 0) return;

    HINTERNET hInit, hConn, hFile;

    hInit = InternetOpenA("HTTPGET", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    hConn = InternetConnectA(hInit, "localhost", REMOTE_DEBUGGING_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    hFile = HttpOpenRequestA(hConn, NULL, "/json", "HTTP/1.1", NULL, NULL,
        INTERNET_FLAG_NO_COOKIES | INTERNET_FLAG_NO_UI | INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTPS , NULL);

    if (HttpSendRequestA(hFile, NULL, 0, NULL, 0)) {
        CHAR content[1024];
        DWORD contentLength = 0;

        InternetReadFile(hFile, content, 1024, &contentLength);

        if (contentLength > 0) {
            static CHAR pattern[] = "\"devtoolsFrontendUrl\": \"";
            auto pos = strstr(content, pattern);

            if (pos) {
                auto start = pos + sizeof(pattern) - 1;
                auto end = strstr(start, "\"");

                std::string link = "http://localhost:";
                link.append(std::to_string(REMOTE_DEBUGGING_PORT));
                link.append(std::string(start, end - start));

                REMOTE_DEVTOOLS_URL = link;
            }
        }
    }

    InternetCloseHandle(hInit);
    InternetCloseHandle(hConn);
    InternetCloseHandle(hFile);
}

void OpenDevTools(BOOL remote)
{
    if (remote) {
        if (REMOTE_DEBUGGING_PORT == 0) return;
        if (REMOTE_DEVTOOLS_URL.empty()) return;

        ShellExecuteA(NULL, "open",
            REMOTE_DEVTOOLS_URL.c_str(), NULL, NULL, SW_SHOWNORMAL);
    }
    else if (CLIENT_BROWSER != nullptr) {
        cef_window_info_t wi{};
        wi.x = CW_USEDEFAULT;
        wi.y = CW_USEDEFAULT;
        wi.width = CW_USEDEFAULT;
        wi.height = CW_USEDEFAULT;
        wi.style = WS_OVERLAPPEDWINDOW
            | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE;
        wi.window_name = CefStr("DevTools - League Client");
        wi.parent_window = GetDesktopWindow();

        auto host = CLIENT_BROWSER->get_host(CLIENT_BROWSER);
        host->show_dev_tools(host, &wi, NULL, NULL, NULL);
        //                              ^--- We use null for client to keep DevTools
        //                                   from being scaled by League Client (e.g 0.8, 1.6).
    }
}

// Use CreateRemoteThread to call this function,
//   to open built-in DevTools from outside.
EXTERN_C __declspec(dllexport) void _OpenDevTools(BOOL remote)
{
    OpenDevTools(remote);
}