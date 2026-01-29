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
        readonly int _webPort;
        readonly bool _vite;
        readonly DevTools _devTools;

        private bool _connected = false;
        private string? _frontEndUrl;
        private string? _webSocketUrl;

        public Debugger(int port, int webPort, bool vite)
        {
            _port = port;
            _webPort = webPort;
            _vite = vite;
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
                    Log.Error("Failed to connect to Riot Client debugger:", ex);
                    break;
                }

                await Task.Delay(delayMs);
            }
        }

        private async Task Initialize(string url)
        {
            _connected = true;

            Log.Debug("Connected to Riot Client debugger");
            Log.Debug("Frontend URL: {0}", url);

            // Intercept the main page to inject our scripts
            await _devTools.InterceptResponse(url, async (url, resp) =>
            {
                var patch = new Utils.HtmlPatcher(resp.body!)
                    // Allow loading from our server
                    .AddCspSource($"http://localhost:{_webPort}")
                    // Inject our global config
                    .AddScriptCode($$"""
                        window.__riot = {
                            appPort: {{Services.AppPort}},
                            authToken: `{{Services.AuthToken}}`,
                        };
                        window.__pengu = {
                            webPort: {{_webPort}},
                        };
                        document.addEventListener('DOMContentLoaded', () => {
                            document.documentElement.dataset.theme = '{{Config.I.riot_theme ?? ""}}';
                        });
                        """, false);

                if (_vite)
                {
                    patch
                        // Vite HMR client
                        .AddScriptTag($"http://localhost:{_webPort}/@vite/client", module: true)
                        // Main Vite script
                        .AddScriptTag($"http://localhost:{_webPort}/src/index.tsx", module: true);

                    Log.Info("Vite mode enabled: Injecting Vite HMR client and scripts");
                }
                else
                {
                    patch
                        // Built main script
                        .AddScriptTag($"http://localhost:{_webPort}/assets/index.js", module: true)
                        // Built stylesheet
                        .AddStyleTag($"http://localhost:{_webPort}/assets/index.css");

                    Log.Info("Production mode enabled: Injecting built scripts");
                }

                if (Config.I.riot_potato_mode)
                {
                    patch.AddStyleCode("""
                        *:not(.campaign-button-wrapper), *::before, *::after {
                          transition: none !important;
                          transition-property: none !important;
                          animation-delay: 0s !important;
                          animation-duration: 0s !important;
                        }
                        """);
                    Log.Info("Riot Client Potato Mode enabled!");
                }

                resp.body = patch.Html;
                Log.Debug("Patched index.html response");

                await ExposeIpc();
            });

            // Reload the page to apply changes
            await _devTools.ReloadPage();

            if (Config.I.riot_disable_sentry)
            {
                await _devTools.SendMethod("Network.enable");
                await _devTools.BlockUrls(["sentry-ipc://sentry-electron.scope/*"]);
                Log.Info("Riot Client Sentry blocking enabled!");
            }
        }

        private async Task ExposeIpc()
        {
            // Add the binding for the internal send function
            await _devTools.RegisterJsBinding("__pengu_ipc__", HandleIpcRequest);

            // Inject JavaScript to define the async window.__ipc function
            string jsCode = """
            (function() {
                if (window.__penguIpc) return;

                const ipcSend = window.__pengu_ipc__;
                delete window.__pengu_ipc__;

                window.__penguIpc = new class {
                    #id = 0;
                    #promises = new Map();

                    resolve(id, data) {
                        const p = this.#promises.get(id);
                        if (p) {
                            p.resolve(data);
                            this.#promises.delete(id);
                        }
                    }

                    reject(id, error) {
                        const p = this.#promises.get(id);
                        if (p) {
                            p.reject(new Error(error));
                            this.#promises.delete(id);
                        }
                    }

                    async send(type, ...args) {
                        const id = ++this.#id;
                        const p = new Promise((resolve, reject) => {
                            this.#promises.set(id, {resolve, reject});
                        });
                        try {
                            const data = { id, type, args };
                            ipcSend(JSON.stringify(data));
                        } catch (e) {
                            this.#promises.delete(id);
                            throw e;
                        }
                        return p;
                    }
                };
            })();
            """;
            await _devTools.EvaluateScript(jsCode);

            Log.Debug("Exposed __penguIpc to browser runtime");
        }

        private async Task HandleIpcRequest(string cmd, string payload)
        {
            using var json = JsonDocument.Parse(payload);
            var root = json.RootElement;

            long requestId = root.GetProperty("id").GetInt64();
            var type = root.GetProperty("type").GetString();
            using var args = root.GetProperty("args").EnumerateArray();

            try
            {
                // Send the resolution back to JS
                object result = await HandleIpcCommand(type!, args);
                var jsExpr = $"window.__penguIpc.resolve({requestId}, {result})";

                await _devTools.EvaluateScript(jsExpr);
            }
            catch (Exception ex)
            {
                // Send the rejection back to JS
                var error = JsValue.String(ex.Message);
                var jsExpr = $"window.__penguIpc.reject({requestId}, {error})";

                await _devTools.EvaluateScript(jsExpr);
            }
        }

        private async Task<JsValue> HandleIpcCommand(string type, JsonElement.ArrayEnumerator args)
        {
            throw new NotImplementedException($"IPC command '{type}' is not implemented.");
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
                Log.Debug("Reloading Riot Client page...");
                _ = _devTools.ReloadPage();
            }
            else
            {
                Log.Debug("Cannot reload Riot Client page: Not connected to debugger.");
            }
        }

        public void OpenRemoteDevTools()
        {
            if (_connected)
            {
                Log.Debug("Opening Riot Client DevTools in browser...");
                Utils.Shell.OpenUrlAsBrowserApp($"http://127.0.0.1:{_port}{_frontEndUrl}");
            }
            else
            {
                Log.Debug("Cannot open Riot Client DevTools: Not connected to debugger.");
            }
        }
    }
}