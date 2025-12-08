using System;
using System.IO;
using Microsoft.Extensions.Logging;
using ZLogger;

namespace Pengu.Loader
{
    static class Logger
    {
        private static readonly ILoggerFactory s_factory;
        private static readonly ILogger s_logger;

        static Logger()
        {
            s_factory = LoggerFactory.Create(builder =>
            {
                builder
                    .ClearProviders()
                    .AddZLoggerFile(Path.Combine(Config.UserDir, "debug.log"), options =>
                    {
                        options.UsePlainTextFormatter(fmt =>
                        {
                            fmt.SetPrefixFormatter($"{0:yyyy-MM-dd HH:mm:ss.fff} [{1:short}] ",
                                (in MessageTemplate template, in LogInfo info) =>
                                {
                                    template.Format(info.Timestamp, info.LogLevel);
                                });
                        });
                    })
                    .AddZLoggerConsole(options =>
                    {
                        options.UsePlainTextFormatter(fmt =>
                        {
                            fmt.SetPrefixFormatter($"{0}{2:HH:mm:ss} [{3:short}]{1} ",
                                (in MessageTemplate template, in LogInfo info) =>
                                {
                                    var cc = info.LogLevel switch
                                    {
                                        LogLevel.Error => "\e[31m",
                                        LogLevel.Information => "\e[32m",
                                        LogLevel.Debug => "\e[36m",
                                        _ => ""
                                    };

                                    template.Format(cc, cc == "" ? "" : "\e[0m", info.Timestamp, info.LogLevel);
                                });
                        });
                    })
                    .SetMinimumLevel(LogLevel.Debug);
            });

            s_logger = s_factory.CreateLogger("PENGU");
        }

        public static void Info(string fmt, params object[] args)
        {
            s_logger.LogInformation(fmt, args);
        }

        public static void Debug(string fmt, params object[] args)
        {
            s_logger.LogDebug(fmt, args);
        }

        public static void Error(string message, Exception? ex = null)
        {
            if (ex is null)
                s_logger.LogError(message);
            else
                s_logger.LogError(ex, message);
        }

        public static void Shutdown()
        {
            s_logger.ZLogInformation($"========================================\n\n");
            s_factory.Dispose();
        }
    }
}