using System;
using System.IO;
using System.Runtime.InteropServices;
using Microsoft.Extensions.Logging;
using ZLogger;

namespace Pengu.Loader
{
    static partial class Log
    {
        private static readonly ILoggerFactory s_factory;
        private static readonly ILogger s_logger;

        static Log()
        {
#if WINDOWS
            // Enable ANSI escape codes on Windows console
            WindowsConsole.EnableAnsi();
#endif

            int pid = Environment.ProcessId;
            var dir = Path.Combine(Config.BaseDir, "logs");
            var path = Path.Combine(dir, string.Format("{0:yyyy-MM-ddTHH-mm-ss}_rc_p{1}.log", DateTime.Now, pid));

            s_factory = LoggerFactory.Create(builder =>
            {
                builder
                    .ClearProviders()
                    .AddZLoggerFile(path, options =>
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
                            fmt.SetPrefixFormatter($"\u001b[49;7m {2:HH:mm:ss.fff} \u001b[0m{0} {3:short} {1} ",
                                (in MessageTemplate template, in LogInfo info) =>
                                {
                                    var cc = info.LogLevel switch
                                    {
                                        LogLevel.Error => "\x1b[101;30m",
                                        LogLevel.Information => "\x1b[102;30m",
                                        LogLevel.Warning => "\x1b[103;30m",
                                        LogLevel.Debug => "\x1b[104;30m",
                                        _ => ""
                                    };

                                    template.Format(cc, cc == "" ? "" : "\x1b[0m", info.Timestamp, info.LogLevel);
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

        public static void Warn(string fmt, params object[] args)
        {
            s_logger.LogWarning(fmt, args);
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

#if WINDOWS
        static partial class WindowsConsole
        {
            [LibraryImport("kernel32.dll")]
            private static partial IntPtr GetStdHandle(int nStdHandle);

            [LibraryImport("kernel32.dll")]
            private static partial int GetConsoleMode(IntPtr hConsoleHandle, out int lpMode);

            [LibraryImport("kernel32.dll")]
            private static partial int SetConsoleMode(IntPtr hConsoleHandle, int dwMode);

            public static void EnableAnsi()
            {
                const int STD_OUTPUT_HANDLE = -11;
                const int ENABLE_VIRTUAL_TERMINAL_PROCESSING = 0x0004;

                var handle = GetStdHandle(STD_OUTPUT_HANDLE);
                GetConsoleMode(handle, out int mode);
                SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
            }
        }
#endif
    }
}