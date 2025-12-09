using System;
using System.Net;
using System.Net.Sockets;

namespace Pengu.Loader.Utils
{
    static class Network
    {
        public static IDisposable GetFreeTcpPort(out int port)
        {
            var l = new TcpListener(IPAddress.Loopback, 0);
            l.Start();
            port = ((IPEndPoint)l.LocalEndpoint).Port;
            return new TcpPortHolder(l);
        }

        private class TcpPortHolder : IDisposable
        {
            private TcpListener listener;
            public TcpPortHolder(TcpListener listener)
            {
                this.listener = listener;
            }
            public void Dispose()
            {
                listener.Stop();
            }
        }
    }
}