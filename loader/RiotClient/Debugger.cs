using System;
using System.Collections.Generic;
using System.Net.Http;
using System.Net.Sockets;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading.Tasks;

namespace Pengu.Loader.RiotClient
{
    partial class Debugger
    {
        int Port;

        public string? FrontEndUrl { get; private set; }
        public string? WebSocketUrl { get; private set; }

        public DevTools DevTools;

        public Debugger(int port)
        {
            this.Port = port;
            this.DevTools = new DevTools();
        }

        public async Task Connect()
        {
            using var client = new HttpClient(new SocketsHttpHandler
            {
                // Fail fast on TCP connect attempts
                ConnectTimeout = TimeSpan.FromSeconds(1),

            }, disposeHandler: true);

            var url = $"http://127.0.0.1:{Port}/json";
            int delayMs = 100;

            while (true)
            {
                // Ensure the connection ASAP
                try
                {
                    using var tcp = new TcpClient();
                    var connectTask = tcp.ConnectAsync("127.0.0.1", Port);
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

                        FrontEndUrl = item!.devtoolsFrontendUrl;
                        WebSocketUrl = item!.webSocketDebuggerUrl;

                        await DevTools.Connect(WebSocketUrl);
                        break;
                    }
                }
                catch
                {
                }

                await Task.Delay(delayMs);
            }
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

        public void OpenRemoteDevTools()
        {
            Utils.Shell.OpenUrlAsBrowserApp($"http://localhost:{Port}{FrontEndUrl}");
        }
    }
}