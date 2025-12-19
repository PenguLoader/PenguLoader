using System;
using System.IO;
using System.Net;
using System.Threading;
using System.Threading.Tasks;

namespace Pengu.Loader.App
{
    public class WebServer : IDisposable
    {
        private CancellationTokenSource _cts;
        private readonly HttpListener _listener;

        public WebServer(int port)
        {
            _cts = new CancellationTokenSource();
            _listener = new HttpListener();

            var prefix = $"http://localhost:{port}/";
            _listener.Prefixes.Add(prefix);
        }

        public void Dispose()
        {
            _listener.Close();
            _cts.Dispose();
        }

        public void Listen()
        {
            Task.Run(() => StartAsync(_cts.Token));
        }

        public void Stop()
        {
            _cts.Cancel();

            if (_listener.IsListening)
                _listener.Stop();
        }

        private async Task StartAsync(CancellationToken token)
        {
            _listener.Start();

            while (!token.IsCancellationRequested)
            {
                var ctx = await _listener.GetContextAsync();
                var url = ctx.Request.Url;

                if (url is null)
                {
                    ctx.Response.StatusCode = 400;
                    ctx.Response.Close();
                    continue;
                }

                var method = ctx.Request.HttpMethod;
                var localPath = url.LocalPath;
                var query = url.Query;

                if (localPath == "/api/fs/read" && method == "GET")
                {
                    await HandlePluginFsRead(ctx, query);
                }
                else if (localPath == "/api/fs/write" && method == "POST")
                {
                    await HandlePluginFsWrite(ctx, query);
                }
                else if (WebAssets.TryGetFile(localPath, out var stream, out var contentType) && method == "GET")
                {
                    ctx.Response.StatusCode = 200;
                    ctx.Response.ContentType = contentType;
                    ctx.Response.ContentLength64 = stream!.Length;
                    ctx.Response.AddHeader("Access-Control-Allow-Origin", "*");

                    using (stream)
                    {
                        await stream!.CopyToAsync(ctx.Response.OutputStream);
                    }

                    ctx.Response.Close();
                }
                else
                {
                    ctx.Response.StatusCode = 404;
                    ctx.Response.Close();
                }
            }
        }

        private async Task HandlePluginFsRead(HttpListenerContext ctx, string query)
        {
            var path = GetQueryParam(query, "path");
            if (string.IsNullOrEmpty(path))
            {
                ctx.Response.StatusCode = 400;
                ctx.Response.Close();
                return;
            }

            try
            {
                if (!File.Exists(path))
                {
                    ctx.Response.StatusCode = 404;
                    ctx.Response.Close();
                    return;
                }

                string ext = Path.GetExtension(path).ToLower();
                if (!MimeTypes.TryGetMimeType(ext, out var contentType))
                    contentType = "application/octet-stream";

                using var stream = File.OpenRead(path);
                ctx.Response.StatusCode = 200;
                ctx.Response.ContentType = contentType;
                ctx.Response.ContentLength64 = stream.Length;
                ctx.Response.AddHeader("Access-Control-Allow-Origin", "*");

                await stream.CopyToAsync(ctx.Response.OutputStream);
                ctx.Response.Close();
            }
            catch (Exception)
            {
                ctx.Response.StatusCode = 500;
                ctx.Response.Close();
            }
        }

        private async Task HandlePluginFsWrite(HttpListenerContext ctx, string query)
        {
            var path = GetQueryParam(query, "path");
            if (string.IsNullOrEmpty(path))
            {
                ctx.Response.StatusCode = 400;
                ctx.Response.Close();
                return;
            }

            try
            {
                using var inputStream = ctx.Request.InputStream;
                using var outputStream = File.Create(path);
                await inputStream.CopyToAsync(outputStream);

                ctx.Response.StatusCode = 200;
                ctx.Response.AddHeader("Access-Control-Allow-Origin", "*");
                ctx.Response.Close();
            }
            catch (Exception)
            {
                ctx.Response.StatusCode = 500;
                ctx.Response.Close();
            }
        }

        private static string? GetQueryParam(string query, string key)
        {
            if (string.IsNullOrEmpty(query)) return null;
            var pairs = query.TrimStart('?').Split('&');
            foreach (var pair in pairs)
            {
                var parts = pair.Split('=');
                if (parts.Length == 2 && parts[0] == key)
                {
                    return Uri.UnescapeDataString(parts[1]);
                }
            }
            return null;
        }
    }
}