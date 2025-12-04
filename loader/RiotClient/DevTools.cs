using System;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading.Tasks;
using Websocket.Client;

namespace Pengu.Loader.RiotClient
{
    partial class DevTools
    {
        WebsocketClient Client;

        int _IdCount = 1;
        object _Lock = new object();
        int NextId
        {
            get
            {
                lock (_Lock)
                {
                    return _IdCount++;
                }
            }
        }

        public DevTools()
        {
            Client = new WebsocketClient(new Uri("ws://_"));
            Client.ReconnectTimeout = TimeSpan.FromSeconds(30);
            Client.MessageReceived.Subscribe(msg =>
            {
                Task.Run(() =>
                {
                    //Native.MessageBox(0, msg.Text ?? "<fail>", "msg", 0);
                });
            });
        }

        public async Task Connect(string debuggerUrl)
        {
            Client.Url = new Uri(debuggerUrl);
            await Client.StartOrFail();
        }

        record Page_setBypassCSP(bool enabled);
        record Page_reload(bool ignoreCache);
        record Runtime_evaluate(string expression);


        [JsonSerializable(typeof(Page_setBypassCSP))]
        [JsonSerializable(typeof(Page_reload))]
        [JsonSerializable(typeof(Runtime_evaluate))]
        partial class DevToolsJsonContext : JsonSerializerContext
        {
        }

        string CreatePayloadJson<T>(int id, T @params)
        {
            var method = typeof(T).Name.Replace('_', '.');
            var jsonParams = JsonSerializer.Serialize(@params, typeof(T), DevToolsJsonContext.Default);
            return $"{{ \"id\": {id}, \"method\": \"{method}\", \"params\": {jsonParams} }}";
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
    }
}