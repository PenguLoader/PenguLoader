using System;
using System.Net;
using System.Net.Sockets;
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

                if (url is null || !WebAssets.TryGetFile(url.LocalPath, out var stream, out var contentType))
                {
                    ctx.Response.StatusCode = 404;
                    ctx.Response.Close();
                    continue;
                }

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
        }

        public static int GetFreePort()
        {
            var listener = new TcpListener(IPAddress.Loopback, 0);
            listener.Start();
            int port = ((IPEndPoint)listener.LocalEndpoint).Port;
            listener.Stop();
            return port;
        }
    }
}