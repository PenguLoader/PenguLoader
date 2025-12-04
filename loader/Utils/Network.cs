using System;
using System.Net;
using System.Net.Sockets;

namespace Pengu.Loader.Utils
{
    static class Network
    {
        public static int GetFreeTcpPort()
        {
            var l = new TcpListener(IPAddress.Loopback, 0);
            l.Start();
            int port = ((IPEndPoint)l.LocalEndpoint).Port;
            l.Stop();
            return port;
        }
    }
}