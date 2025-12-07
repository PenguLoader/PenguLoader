using System;

namespace Pengu.Loader.RiotClient
{
    static class Services
    {
        public static int AppPort { get; }
        public static string? AuthToken { get; }

        static Services()
        {
            // --app-port=12345 --remoting-auth-token=abcdefg
            const string prefixPort = "--app-port=";
            const string prefixToken = "--remoting-auth-token=";

            var env = Environment.CommandLine!;
            var parts = env.Split(' ');

            foreach (var part in parts)
            {
                if (part.StartsWith(prefixPort))
                {
                    var portStr = part.Substring(prefixPort.Length);
                    if (int.TryParse(portStr, out var port))
                    {
                        AppPort = port;
                    }
                }
                else if (part.StartsWith(prefixToken))
                {
                    AuthToken = part.Substring(prefixToken.Length);
                }
            }

            // TODO: this approach only supports inside the Riot Client process.
            // For remote process, use devtools method SystemInfo.getProcessInfo
        }
    }
}