using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

namespace LeagueLoader
{
    internal class Dll
    {
        static string D3d9DllPath => Path.Combine(Directory.GetCurrentDirectory(), "d3d9.dll");

        [DllImport("kernel32.dll", EntryPoint = "CreateSymbolicLinkW", CharSet = CharSet.Unicode)]
        static extern int CreateSymbolicLink(string lpSymlinkFileName, string lpTargetFileName, uint dwFlags = 0);

        [DllImport("kernel32.dll")]
        static extern IntPtr CreateRemoteThread(IntPtr hProcess,
            IntPtr lpThreadAttributes, uint dwStackSize, IntPtr lpStartAddress,
            IntPtr lpParameter, uint dwCreationFlags, IntPtr lpThreadId);

        [DllImport("kernel32.dll")]
        static extern int GetExitCodeThread(IntPtr hThread, out uint lpExitCode);

        [DllImport("kernel32.dll")]
        static extern IntPtr VirtualAllocEx(IntPtr hProcess,
            IntPtr lpAddress, UIntPtr dwSize, uint flAllocationType, uint flProtect);

        [DllImport("kernel32.dll")]
        static extern int WriteProcessMemory(IntPtr hProcess,
            IntPtr lpBaseAddress, byte[] buffer, UIntPtr nSize, IntPtr lpNumberOfBytesWritten);

        [DllImport("kernel32.dll", CharSet = CharSet.Ansi)]
        static extern IntPtr LoadLibraryExA(string lpLibFileName, IntPtr reserved, uint flags);

        [DllImport("kernel32.dll", CharSet = CharSet.Ansi)]
        static extern IntPtr GetProcAddress(IntPtr module, string name);

        [DllImport("kernel32.dll", CharSet = CharSet.Ansi)]
        static extern IntPtr GetModuleHandleA(string name);

        [DllImport("kernel32.dll")]
        static extern uint WaitForSingleObject(IntPtr hHandle, uint dwMilliseconds);

        const uint INFINITE = 0xFFFFFFFF;
        const uint DONT_RESOLVE_DLL_REFERENCES = 0x00000001;
        const uint MEM_COMMIT = 0x00001000;
        const uint MEM_RESERVE = 0x00002000;
        const uint PAGE_READWRITE = 0x04;

        public static void OpenDevTools(bool remote)
        {
            var processes = Process.GetProcessesByName("LeagueClientUx");
            if (processes.Length > 0)
            {
                // We have prepared _OpenDevTools() in d3d9.dll.
                // And now, we will find its address in LeagueClientUx.exe.

                var lcux = processes[0].Handle;

                // Refer to https://stackoverflow.com/q/26395243, comment 1.
                var kernel32 = GetModuleHandleA("kernel32.dll");
                var GMHA = GetProcAddress(kernel32, "GetModuleHandleA");

                byte[] bytes = Encoding.ASCII.GetBytes("d3d9.dll");
                var addr = VirtualAllocEx(lcux, (IntPtr)0, (UIntPtr)bytes.Length, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
                WriteProcessMemory(lcux, addr, bytes, (UIntPtr)bytes.Length, (IntPtr)0);

                var thread = CreateRemoteThread(lcux, (IntPtr)0, 0, GMHA, addr, 0, (IntPtr)0);
                WaitForSingleObject(thread, INFINITE);

                // Get result of GetModuleHandleA().
                // This method works fine in 32-bit process, but not in 64-bit.
                uint lcuxD9d9;
                GetExitCodeThread(thread, out lcuxD9d9);

                // Refer to https://stackoverflow.com/a/26397667, solution 2.
                var myD3d9 = LoadLibraryExA("./d3d9.dll", IntPtr.Zero, DONT_RESOLVE_DLL_REFERENCES);
                var myProc = GetProcAddress(myD3d9, "_OpenDevTools");
                var lcuxProc = (uint)myProc - (uint)myD3d9 + lcuxD9d9;

                // Finally, invoke it.
                CreateRemoteThread(lcux, (IntPtr)0, 0, (IntPtr)lcuxProc, (IntPtr)Convert.ToInt32(remote), 0, (IntPtr)0);
            }
        }

        // For d3d9.dll in both League Loader and League Client are in same folder or symlink.
        public static void OpenDevTools_(bool remote)
        {
            var processes = Process.GetProcessesByName("LeagueClientUx");
            if (processes.Length > 0)
            {
                var lcux = processes[0].Handle;

                var d3d9 = LoadLibraryExA("./d3d9.dll", IntPtr.Zero, DONT_RESOLVE_DLL_REFERENCES);
                var proc = GetProcAddress(d3d9, "_OpenDevTools");

                CreateRemoteThread(lcux, (IntPtr)0, 0, proc, (IntPtr)Convert.ToInt32(remote), 0, (IntPtr)0);
            }
        }

        public static bool Exist()
        {
            return File.Exists(D3d9DllPath);
        }

        public static bool IsLoaded()
        {
            var procs = Process.GetProcessesByName("LeagueClientUx");
            if (procs.Length > 0)
            {
                var lcux = procs[0];
                var lcuxDir = Directory.GetParent(lcux.MainModule.FileName).FullName;
                var dllPath = Path.Combine(lcuxDir, "d3d9.dll");

                foreach (ProcessModule module in lcux.Modules)
                {
                    if (string.Equals(module.FileName, dllPath, StringComparison.OrdinalIgnoreCase))
                    {
                        return true;
                    }
                }
            }

            return false;
        }

        public static bool IsInstalled(string lcDir)
        {
            if (string.IsNullOrEmpty(lcDir)) return false;

            var linkPath = Path.Combine(lcDir, "d3d9.dll");
            if (!File.Exists(linkPath)) return false;

            var info = new FileInfo(linkPath);
            return info.Attributes.HasFlag(FileAttributes.ReparsePoint);
        }

        public static void Install(string lcDir)
        {
            if (!IsInstalled(lcDir))
            {
                var linkPath = Path.Combine(lcDir, "d3d9.dll");
                CreateSymbolicLink(linkPath, D3d9DllPath);
            }
        }

        public static void Uninstall(string lcDir)
        {
            if (IsInstalled(lcDir))
            {
                var linkPath = Path.Combine(lcDir, "d3d9.dll");
                File.Delete(linkPath);
            }
        }
    }
}