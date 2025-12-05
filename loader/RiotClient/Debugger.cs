using System;
using System.Collections.Generic;
using System.Net.Http;
using System.Net.Sockets;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading.Tasks;

namespace Pengu.Loader.RiotClient
{
    partial class Debugger : IDisposable
    {
        readonly int _port;
        readonly DevTools _devTools;

        private bool _connected = false;
        private string? _frontEndUrl;
        private string? _webSocketUrl;

        public Debugger(int port)
        {
            _port = port;
            _devTools = new DevTools();
        }

        public void Dispose()
        {
            _devTools.Dispose();
        }

        public async Task Connect()
        {
            using var client = new HttpClient(new SocketsHttpHandler
            {
                // Fail fast on TCP connect attempts
                ConnectTimeout = TimeSpan.FromSeconds(1),

            }, disposeHandler: true);

            var url = $"http://127.0.0.1:{_port}/json";
            int delayMs = 100;

            while (true)
            {
                // Ensure the connection ASAP
                try
                {
                    using var tcp = new TcpClient();
                    var connectTask = tcp.ConnectAsync("127.0.0.1", _port);
                    await connectTask.WaitAsync(TimeSpan.FromSeconds(5));
                    // If we get here, the port accepted TCP connection
                }
                catch
                {
                    // Not listening yet? back off and retry
                    await Task.Delay(delayMs);
                    delayMs = Math.Min(1000, delayMs * 2);
                    continue;
                }

                try
                {
                    var json = await client.GetStringAsync(url);
                    var list = JsonSerializer.Deserialize(json, DebuggerJsonSerializer.Default.ListDebuggerItem);

                    if (list != null && list.Count > 0)
                    {
                        var item = list!.Find(e => e.title == "Riot Client" && e.type == "page");

                        _frontEndUrl = item!.devtoolsFrontendUrl;
                        _webSocketUrl = item!.webSocketDebuggerUrl;

                        await _devTools.Connect(_webSocketUrl);
                        await Initialize(item.url);

                        break;
                    }
                }
                catch (Exception ex)
                {
                    Logger.Error("Failed to connect to Riot Client debugger:", ex);
                    break;
                }

                await Task.Delay(delayMs);
            }
        }

        private async Task Initialize(string url)
        {
            _connected = true;

            Logger.Debug("Connected to Riot Client debugger");
            Logger.Debug("Frontend URL: {0}", url);

            // Intercept the main page to inject our scripts
            await _devTools.InterceptResponse(url, (url, resp) =>
            {
                var patch = new Utils.HtmlPatcher(resp.body!)
                    // Allow loading scripts from Vite dev server
                    .AddCspSource("http://localhost:3000")
                    // Inject Vite HMR client and our main script
                    .AddScriptTag("http://localhost:3000/@vite/client", module: true)
                    // Inject our main script
                    .AddScriptTag("http://localhost:3000/src/index.tsx", module: true);

                resp.body = patch.Html;
                return Task.CompletedTask;
            });

            // Reload the page to apply changes
            await _devTools.ReloadPage();
        }

        record DebuggerItem(
            string title,
            string type,
            string url,
            string webSocketDebuggerUrl,
            string devtoolsFrontendUrl
        );

        [JsonSerializable(typeof(DebuggerItem))]
        [JsonSerializable(typeof(List<DebuggerItem>))]
        partial class DebuggerJsonSerializer : JsonSerializerContext
        {
        }

        public void ReloadPage()
        {
            if (_connected)
            {
                Logger.Debug("Reloading Riot Client page...");
                _ = _devTools.ReloadPage();
            }
            else
            {
                Logger.Debug("Cannot reload Riot Client page: Not connected to debugger.");
            }
        }

        public void OpenRemoteDevTools()
        {
            if (_connected)
            {
                Logger.Debug("Opening Riot Client DevTools in browser...");
                Utils.Shell.OpenUrlAsBrowserApp($"http://127.0.0.1:{_port}{_frontEndUrl}");
            }
            else
            {
                Logger.Debug("Cannot open Riot Client DevTools: Not connected to debugger.");
            }
        }

        public void BlockSentry()
        {
            if (_connected)
            {
                Logger.Debug("Blocking Sentry requests...");
                _ = _devTools.BlockUrls(["sentry-ipc://sentry-electron.scope/*"]);
            }
            else
            {
                Logger.Debug("Cannot block Sentry requests: Not connected to debugger.");
            }
        }
    }
}