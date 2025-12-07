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
            await _devTools.InterceptResponse(url, async (url, resp) =>
            {
                var patch = new Utils.HtmlPatcher(resp.body!)
                    // Allow loading scripts from Vite dev server
                    .AddCspSource("http://localhost:3000")
                    // Inject our global config
                    .AddScriptCode($$"""
                        window.__riot = {
                            appPort: {{Services.AppPort}},
                            authToken: `{{Services.AuthToken}}`,
                        };
                        """, false)
                    // Inject Vite HMR client and our main script
                    .AddScriptTag("http://localhost:3000/@vite/client", module: true)
                    // Inject our main script
                    .AddScriptTag("http://localhost:3000/src/index.tsx", module: true);

                resp.body = patch.Html;
                Logger.Debug("Patched index.html response");

                await ExposeIpc();
            });

            // Reload the page to apply changes
            await _devTools.ReloadPage();
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

            Logger.Debug("Exposed __penguIpc to browser runtime");
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