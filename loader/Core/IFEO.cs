using System;
using System.ComponentModel;
using System.Diagnostics;
using Microsoft.Win32;

namespace Pengu.Loader.Core
{
    static class IFEO
    {
        static string IFEO_PATH => @"SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options";
        static string VALUE_NAME => "Debugger";

        public static string? GetDebugger(string target)
        {
            using (var key = Registry.LocalMachine.OpenSubKey(IFEO_PATH))
            {
                if (key == null)
                    return string.Empty;

                using (var image = key.OpenSubKey(target))
                {
                    if (image == null)
                        return string.Empty;

                    return image.GetValue(VALUE_NAME) as string;
                }
            }
        }

        public static void SetDebugger(string target, string debugger)
        {
            debugger = debugger.Replace("\"", "\\\"");
            RunRegCommand($"add \"HKLM\\{IFEO_PATH}\\{target}\" /v \"{VALUE_NAME}\" /t REG_SZ /d \"{debugger}\" /f");
        }

        public static void RemoveDebugger(string target)
        {
            RunRegCommand($"delete \"HKLM\\{IFEO_PATH}\\{target}\" /f");
        }

        /// <summary>
        /// Executes a reg command with elevated privileges.
        /// </summary>
        /// <remarks>
        /// Should use external reg.exe tool to modify registry with admin rights.
        /// Modifying registry keys directly via RegistryKey class may trigger AV.
        /// </remarks>
        /// <exception cref="InvalidOperationException">Thrown if the command fails for any reason other than access denied.</exception>
        /// <exception cref="UnauthorizedAccessException">Thrown if access is denied or the operation is canceled by the user.</exception>
        public static void RunRegCommand(string args)
        {
            try
            {
                using (var process = new Process
                {
                    StartInfo = new ProcessStartInfo
                    {
                        FileName = "cmd.exe",
                        Arguments = $"/C reg {args}",
                        UseShellExecute = true,
                        CreateNoWindow = true,
                        WindowStyle = ProcessWindowStyle.Hidden,
                        Verb = "runas",
                    }
                })
                {
                    process.Start();
                    process.WaitForExit();

                    int exitCode = process.ExitCode;

                    if (exitCode != 0)
                    {
                        if (exitCode == 5)
                            throw new UnauthorizedAccessException("Access denied when trying to run reg command.");

                        throw new InvalidOperationException($"Reg command failed with exit code: {exitCode}");
                    }
                }
            }
            catch (Win32Exception ex) when (ex.NativeErrorCode == 1223)
            {
                // The operation was canceled by the user.
                throw new UnauthorizedAccessException("The operation was canceled by the user.", ex);
            }
            catch (Exception ex)
            {
                throw new InvalidOperationException("Failed to execute reg command.", ex);
            }
        }
    }
}