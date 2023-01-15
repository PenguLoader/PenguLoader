#include "internal.h"

#include <wininet.h>
#pragma comment(lib, "wininet.lib")

using namespace league_loader;

// BROWSER PROCESS ONLY.

extern UINT REMOTE_DEBUGGING_PORT;
extern cef_browser_t *CLIENT_BROWSER;
static std::string REMOTE_DEVTOOLS_URL;

#define OPEN_DEVTOOLS_EVENT         "Global\\LeagueLoader.OpenDevTools"
#define OPEN_REMOTE_DEVTOOLS_EVENT  "Global\\LeagueLoader.OpenRemoteDevTools"

static void OpenDevTools_Internal(bool remote)
{
    if (remote)
    {
        if (REMOTE_DEBUGGING_PORT == 0) return;
        if (REMOTE_DEVTOOLS_URL.empty()) return;

        ShellExecuteA(NULL, "open",
            REMOTE_DEVTOOLS_URL.c_str(), NULL, NULL, SW_SHOWNORMAL);
    }
    else if (CLIENT_BROWSER != nullptr)
    {
        cef_window_info_t wi{};
        wi.x = CW_USEDEFAULT;
        wi.y = CW_USEDEFAULT;
        wi.width = CW_USEDEFAULT;
        wi.height = CW_USEDEFAULT;
        wi.style = WS_OVERLAPPEDWINDOW
            | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE;
        wi.window_name = CefStr("DevTools - League Client");
        wi.parent_window = NULL /*GetDesktopWindow()*/;

        auto host = CLIENT_BROWSER->get_host(CLIENT_BROWSER);
        host->show_dev_tools(host, &wi, NULL, NULL, NULL);
        //                              ^--- We use null for client to keep DevTools
        //                                   from being scaled by League Client (e.g 0.8, 1.6).
    }
}

void PrepareDevToolsThread()
{
    if (REMOTE_DEBUGGING_PORT != 0)
    {
        HINTERNET hInit, hConn, hFile;

        hInit = InternetOpenA("HTTPGET", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
        hConn = InternetConnectA(hInit, "localhost", REMOTE_DEBUGGING_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
        hFile = HttpOpenRequestA(hConn, NULL, "/json", "HTTP/1.1", NULL, NULL,
            INTERNET_FLAG_NO_COOKIES | INTERNET_FLAG_NO_UI | INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTPS, NULL);

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

    // Handle remote opener via event signaling.

    HANDLE events[2];
    events[0] = CreateEventA(NULL, TRUE, FALSE, OPEN_DEVTOOLS_EVENT);
    events[1] = CreateEventA(NULL, TRUE, FALSE, OPEN_REMOTE_DEVTOOLS_EVENT);

    while (CLIENT_BROWSER != nullptr)
    {
        // Wait for events.
        DWORD ret = WaitForMultipleObjects(2, events, FALSE, 1000);

        if (ret == WAIT_OBJECT_0)
        {
            OpenDevTools_Internal(false);
            ResetEvent(events[0]);
            continue;
        }
        else if (ret == WAIT_OBJECT_0 + 1)
        {
            OpenDevTools_Internal(true);
            ResetEvent(events[1]);
            continue;
        }

        Sleep(10);
    }
}

// Cross-process call.
void league_loader::OpenDevTools(bool remote)
{
    auto eventName = remote
        ? OPEN_REMOTE_DEVTOOLS_EVENT : OPEN_DEVTOOLS_EVENT;

    if (HANDLE event = OpenEventA(EVENT_MODIFY_STATE, FALSE, eventName))
    {
        SetEvent(event);
        CloseHandle(event);
    }
}