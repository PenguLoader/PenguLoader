using System;
using System.Collections.Generic;
using System.Net.WebSockets;
using System.Reactive.Linq;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using System.Collections.Concurrent;
using Websocket.Client;

namespace Pengu.Loader.RiotClient
{
    partial class DevTools : IDisposable
    {
        readonly WebsocketClient _client;

        private long _nid = 0;
        private long NextId => Interlocked.Increment(ref _nid);

        public class HttpResponse
        {
            public required int status;
            public required IDictionary<string, string> headers;
            public string? body;
        }

        public delegate Task ResponseInterceptor(string url, HttpResponse resp);
        public delegate Task JsBindingHandler(string name, string payload);

        readonly List<(string pattern, ResponseInterceptor intercept)> _responseInterceptors = new();
        readonly ConcurrentDictionary<long, TaskCompletionSource<JsonElement>> _pendingRequests = new();
        readonly ConcurrentDictionary<string, JsBindingHandler> _jsBindings = new();

        public event Action? PageLoaded;

        public DevTools()
        {
            _client = new WebsocketClient(new Uri("ws://_"));
            _client.ReconnectTimeout = null;
            _client.MessageReceived.Subscribe(HandleMessage);
            _client.DisconnectionHappened.Subscribe(info =>
            {
                Log.Debug("DevTools disconnected: {0}", info.Type);
            });
        }

        public void Dispose()
        {
            _client.Dispose();
        }

        public async Task Connect(string debuggerUrl)
        {
            _client.Url = new Uri(debuggerUrl);

            await _client.StartOrFail();
        }

        private void HandleMessage(ResponseMessage msg)
        {
            if (msg.MessageType != WebSocketMessageType.Text)
                return;

            var json = msg.Text;
            if (string.IsNullOrEmpty(json))
                return;

            try
            {
                using var doc = JsonDocument.Parse(json);
                var root = doc.RootElement;

                // If message contains an "id" it's a response to a previously-sent request
                if (root.TryGetProperty("id", out var idEl) && idEl.ValueKind == JsonValueKind.Number)
                {
                    var id = idEl.GetInt64();
                    if (_pendingRequests.TryRemove(id, out var tcs))
                    {
                        if (root.TryGetProperty("result", out var resultEl))
                        {
                            tcs.TrySetResult(resultEl.Clone());
                        }
                        else if (root.TryGetProperty("error", out var errEl))
                        {
                            tcs.TrySetException(new Exception(errEl.ToString()));
                        }
                        else
                        {
                            tcs.TrySetResult(JsonDocument.Parse("{}").RootElement);
                        }
                    }

                    return;
                }

                // Otherwise handle as an event
                if (root.TryGetProperty("method", out var methodEl))
                {
                    var method = methodEl.GetString();

                    // Handle page load event
                    if (method == "Page.loadEventFired")
                    {
                        PageLoaded?.Invoke();
                        return;
                    }

                    // Handle Runtime.bindingCalled (JS -> client binding)
                    if (method == "Runtime.bindingCalled")
                    {
                        try
                        {
                            if (!root.TryGetProperty("params", out var paramsEl))
                                return;

                            var name = paramsEl.GetProperty("name").GetString();
                            var payload = paramsEl.GetProperty("payload").GetString();

                            if (name != null && _jsBindings.TryGetValue(name, out var handler) && handler != null)
                            {
                                // Fire-and-forget to avoid blocking protocol message processing.
                                Task.Run(async () =>
                                {
                                    try
                                    {
                                        await handler(name, payload ?? string.Empty).ConfigureAwait(false);
                                    }
                                    catch (Exception ex)
                                    {
                                        Log.Error("JS binding handler threw", ex);
                                    }
                                });
                            }
                        }
                        catch (Exception ex)
                        {
                            Log.Error("DevTools binding handling failed", ex);
                        }

                        return;
                    }

                    // Handle Fetch.requestPaused for interception
                    if (method == "Fetch.requestPaused")
                    {
                        try
                        {
                            if (!root.TryGetProperty("params", out var paramsEl))
                                return;

                            var requestId = paramsEl.GetProperty("requestId").GetString();
                            var requestUrl = paramsEl.GetProperty("request").GetProperty("url").GetString();

                            if (requestId == null || requestUrl == null)
                                return;

                            // Only handle responses (when a response was received by the fetch agent)
                            if (!paramsEl.TryGetProperty("responseStatusCode", out var statusEl))
                                return;

                            int statusCode = statusEl.GetInt32();

                            // Find matching interceptor
                            var interceptor = _responseInterceptors.FirstOrDefault(i => requestUrl.Contains(i.pattern, StringComparison.OrdinalIgnoreCase));
                            if (interceptor.intercept == null)
                                return;

                            // Continue asynchronously
                            Task.Run(async () =>
                            {
                                try
                                {
                                    await ContinueResponse(requestId, requestUrl, statusCode, interceptor.intercept).ConfigureAwait(false);
                                }
                                catch (Exception ex)
                                {
                                    Log.Error("DevTools ContinueResponse failed", ex);
                                }
                            });
                        }
                        catch (Exception ex)
                        {
                            Log.Error("DevTools interception failed", ex);
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                Log.Error("DevTools failed to parse message", ex);
            }
        }

        private async Task ContinueResponse(string requestId, string requestUrl, int statusCode, ResponseInterceptor interceptor)
        {
            // Ask for the response body
            var getBodyTask = GetResponseBody(requestId, TimeSpan.FromSeconds(10));
            var bodyResult = await getBodyTask.ConfigureAwait(false);

            string? bodyBase64 = null;
            bool base64Encoded = false;
            if (bodyResult.ValueKind == JsonValueKind.Object)
            {
                if (bodyResult.TryGetProperty("body", out var b))
                    bodyBase64 = b.GetString();

                if (bodyResult.TryGetProperty("base64Encoded", out var be))
                    base64Encoded = be.GetBoolean();
            }

            var originalBody = string.Empty;
            if (!string.IsNullOrEmpty(bodyBase64))
            {
                var bytes = base64Encoded ? Convert.FromBase64String(bodyBase64) : System.Text.Encoding.UTF8.GetBytes(bodyBase64);
                originalBody = System.Text.Encoding.UTF8.GetString(bytes);
            }

            // Let's call the interceptor
            var resp = new HttpResponse()
            {
                status = statusCode,
                headers = new Dictionary<string, string>(),
                body = originalBody
            };
            await interceptor(requestUrl, resp);

            var newBody = resp.body ?? string.Empty;
            var newBytes = System.Text.Encoding.UTF8.GetBytes(newBody);
            var newBase64 = Convert.ToBase64String(newBytes);

            // Fulfill request with modified body
            var fulfillParams = new Fetch_fulfillRequest(requestId, statusCode, [], newBase64);
            var fulfillPayload = CreatePayloadJson(NextId, fulfillParams);
            await _client.SendInstant(fulfillPayload);
        }

        record Page_reload(bool ignoreCache);
        record Fetch_enable(RequestPattern[]? patterns);
        record Fetch_getResponseBody(string requestId);
        record HeaderEntry(string name, string value);
        record Fetch_fulfillRequest(string requestId, int responseCode, HeaderEntry[] responseHeaders, string body);
        record RequestPattern(string urlPattern, string requestStage);
        record Network_setBlockedURLs(string[] urls);
        record Runtime_addBinding(string name);
        record Runtime_evaluate(string expression);

        [JsonSerializable(typeof(Page_reload))]
        [JsonSerializable(typeof(Fetch_enable))]
        [JsonSerializable(typeof(Fetch_getResponseBody))]
        [JsonSerializable(typeof(HeaderEntry))]
        [JsonSerializable(typeof(Fetch_fulfillRequest))]
        [JsonSerializable(typeof(Network_setBlockedURLs))]
        [JsonSerializable(typeof(Runtime_addBinding))]
        [JsonSerializable(typeof(Runtime_evaluate))]
        partial class DevToolsJsonContext : JsonSerializerContext
        {
        }

        string CreatePayloadJson<T>(long id, T @params)
        {
            var method = typeof(T).Name.Replace('_', '.');
            var jsonParams = JsonSerializer.Serialize(@params, typeof(T), DevToolsJsonContext.Default);
            return $"{{ \"id\": {id}, \"method\": \"{method}\", \"params\": {jsonParams} }}";
        }

        async Task<JsonElement> GetResponseBody(string requestId, TimeSpan timeout)
        {
            long id = NextId;
            var tcs = new TaskCompletionSource<JsonElement>(TaskCreationOptions.RunContinuationsAsynchronously);
            _pendingRequests[id] = tcs;

            var @params = new Fetch_getResponseBody(requestId);
            var payload = CreatePayloadJson(id, @params);
            await _client.SendInstant(payload);

            using var cts = new CancellationTokenSource(timeout);
            using (cts.Token.Register(() => tcs.TrySetException(new TimeoutException())))
            {
                return await tcs.Task.ConfigureAwait(false);
            }
        }

        public async Task SendMethod(string method)
        {
            var json = $"{{ \"id\": {NextId}, \"method\": \"{method}\", \"params\": {{}} }}";
            await _client.SendInstant(json);
        }

        public async Task ReloadPage(bool ignoreCache = false)
        {
            var @params = new Page_reload(ignoreCache);

            var json = CreatePayloadJson(NextId, @params);
            await _client.SendInstant(json);
        }

        public async Task EvaluateScript(string expression)
        {
            var @params = new Runtime_evaluate(expression);

            var json = CreatePayloadJson(NextId, @params);
            await _client.SendInstant(json);
        }

        public async Task BlockUrls(string[] urls)
        {
            var @params = new Network_setBlockedURLs(urls);
            var json = CreatePayloadJson(NextId, @params);
            await _client.SendInstant(json);
        }

        public async Task InterceptResponse(string pattern, ResponseInterceptor interceptor)
        {
            if (string.IsNullOrEmpty(pattern) || interceptor == null)
                return;

            // Add to interceptors list
            _responseInterceptors.Add((pattern, interceptor));

            // Ensure Fetch domain is enabled with responseHandling
            var @params = new Fetch_enable([new(pattern, "Response")]);
            var json = CreatePayloadJson(NextId, @params);
            await _client.SendInstant(json);
        }

        public async Task RegisterJsBinding(string name, JsBindingHandler handler)
        {
            if (string.IsNullOrEmpty(name) || handler == null)
                return;

            _jsBindings[name] = handler;

            var @params = new Runtime_addBinding(name);
            var json = CreatePayloadJson(NextId, @params);
            await _client.SendInstant(json);
        }
    }
}