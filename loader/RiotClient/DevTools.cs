using System;
using System.Net.WebSockets;
using System.Reactive.Linq;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading;
using System.Threading.Tasks;
using Websocket.Client;

namespace Pengu.Loader.RiotClient
{
    partial class DevTools
    {
        WebsocketClient Client;

        long _IdCount = 0;
        long NextId => Interlocked.Increment(ref _IdCount);

        public DevTools()
        {
            Client = new WebsocketClient(new Uri("ws://_"));
            Client.ReconnectTimeout = TimeSpan.FromSeconds(30);
            Client.MessageReceived.Subscribe(HandleMessage);
        }

        public async Task Connect(string debuggerUrl)
        {
            Client.Url = new Uri(debuggerUrl);
            await Client.StartOrFail();

            await SendMethod("Page.enable");
            await SendMethod("Page.reload");

            //try
            //{
            //    await BlockUrls(
            //    [
            //        "sentry-ipc://sentry-electron.scope/*",
            //                ]);

            //}
            //catch (Exception ex)
            //{
            //    Console.WriteLine("[LOADER_W] DevTools BlockUrls failed: {0}", ex);
            //}

            //await EnableFetchInterception([new("https://example.com", "Request")]);
        }

        private void HandleMessage(ResponseMessage msg)
        {
            if (msg.MessageType != WebSocketMessageType.Text)
                return;

            var json = msg.Text;
            if (string.IsNullOrEmpty(json))
                return;

            var ev = JsonSerializer.Deserialize(json,
                DevToolsJsonContext.Default.MethodOnlyEvent);

            if (ev != null && ev.method == "Page.loadEventFired")
            {
                int port = 3000;
                _ = this.EvaluateScript(
                    $$"""
                    (async () => {
                        console.log("LOADER: Injecting Vite client and app code...");
                        await import(`https://localhost:{{port}}/@vite/client`);
                        await import(`https://localhost:{{port}}/src/index.tsx`);
                    })();
                    """);
            }
        }

        record MethodOnlyEvent(string method);

        record Page_setBypassCSP(bool enabled);
        record Page_reload(bool ignoreCache);
        record Runtime_evaluate(string expression);

        public record RequestPattern(string urlPattern, string requestStage);
        record Fetch_enable(RequestPattern[]? patterns);

        record Network_setBlockedURLs(string[] urls);

        [JsonSerializable(typeof(Page_setBypassCSP))]
        [JsonSerializable(typeof(Page_reload))]
        [JsonSerializable(typeof(Runtime_evaluate))]
        [JsonSerializable(typeof(Fetch_enable))]
        [JsonSerializable(typeof(Network_setBlockedURLs))]
        [JsonSerializable(typeof(MethodOnlyEvent))]
        partial class DevToolsJsonContext : JsonSerializerContext
        {
        }

        string CreatePayloadJson<T>(long id, T @params)
        {
            var method = typeof(T).Name.Replace('_', '.');
            Console.WriteLine("[LOADER_V] DevTools sending id: {0} | method: {1}", id, method);

            var jsonParams = JsonSerializer.Serialize(@params, typeof(T), DevToolsJsonContext.Default);
            return $"{{ \"id\": {id}, \"method\": \"{method}\", \"params\": {jsonParams} }}";
        }

        public async Task SendMethod(string method)
        {
            var json = $"{{ \"id\": {NextId}, \"method\": \"{method}\", \"params\": {{}} }}";
            await Client.SendInstant(json);
        }

        public async Task SetBypassCSP(bool enabled)
        {
            var @params = new Page_setBypassCSP(true);

            var json = CreatePayloadJson(NextId, @params);
            await Client.SendInstant(json);
        }

        public async Task ReloadPage(bool ignoreCache = false)
        {
            var @params = new Page_reload(ignoreCache);

            var json = CreatePayloadJson(NextId, @params);
            await Client.SendInstant(json);
        }

        public async Task EvaluateScript(string expression)
        {
            var @params = new Runtime_evaluate(expression);

            var json = CreatePayloadJson(NextId, @params);
            await Client.SendInstant(json);
        }

        public async Task EnableFetchInterception(RequestPattern[]? patterns = null)
        {
            var @params = new Fetch_enable(patterns);
            var json = CreatePayloadJson(NextId, @params);
            await Client.SendInstant(json);
        }

        public async Task BlockUrls(string[] urls)
        {
            var @params = new Network_setBlockedURLs(urls);
            var json = CreatePayloadJson(NextId, @params);
            await Client.SendInstant(json);
        }
    }
}