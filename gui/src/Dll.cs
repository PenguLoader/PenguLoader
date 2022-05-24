using System;
using System.Text;
using System.Diagnostics;
using System.Runtime.InteropServices;

namespace LeagueLoader
{
    internal class Dll
    {
        [DllImport("kernel32.dll")]
        private static extern IntPtr CreateRemoteThread(IntPtr hProcess,
            IntPtr lpThreadAttributes, uint dwStackSize, IntPtr lpStartAddress,
            IntPtr lpParameter, uint dwCreationFlags, IntPtr lpThreadId);

        [DllImport("kernel32.dll")]
        private static extern int GetExitCodeThread(IntPtr hThread, out uint lpExitCode);

        [DllImport("kernel32.dll")]
        private static extern IntPtr VirtualAllocEx(IntPtr hProcess,
            IntPtr lpAddress, UIntPtr dwSize, uint flAllocationType, uint flProtect);

        [DllImport("kernel32.dll")]
        private static extern int WriteProcessMemory(IntPtr hProcess,
            IntPtr lpBaseAddress, byte[] buffer, UIntPtr nSize, IntPtr lpNumberOfBytesWritten);

        [DllImport("kernel32.dll", CharSet = CharSet.Ansi)]
        private static extern IntPtr LoadLibraryExA(string lpLibFileName, IntPtr reserved, uint flags);

        [DllImport("kernel32.dll", CharSet = CharSet.Ansi)]
        private static extern IntPtr GetProcAddress(IntPtr module, string name);

        [DllImport("kernel32.dll", CharSet = CharSet.Ansi)]
        private static extern IntPtr GetModuleHandleA(string name);

        [DllImport("kernel32.dll")]
        private static extern uint WaitForSingleObject(IntPtr hHandle, uint dwMilliseconds);

        private const uint INFINITE = 0xFFFFFFFF;
        private const uint DONT_RESOLVE_DLL_REFERENCES = 0x00000001;
        private const uint MEM_COMMIT = 0x00001000;
        private const uint MEM_RESERVE = 0x00002000;
        private const uint PAGE_READWRITE = 0x04;

        public static void Open(bool remote)
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
                var addr = VirtualAllocEx(lcux, IntPtr.Zero, (UIntPtr)bytes.Length, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
                WriteProcessMemory(lcux, addr, bytes, (UIntPtr)bytes.Length, IntPtr.Zero);

                var thread = CreateRemoteThread(lcux, IntPtr.Zero, 0, GMHA, addr, 0, IntPtr.Zero);
                WaitForSingleObject(thread, INFINITE);

                // Get result of GetModuleHandleA().
                // This method works fine in 32-bit process, but not in 64-bit.
                uint lcuxD9d9;
                GetExitCodeThread(thread, out lcuxD9d9);

                // Refer to https://stackoverflow.com/a/26397667, solution 2.
                var myD3d9 = LoadLibraryExA("./d3d9.dll", IntPtr.Zero, DONT_RESOLVE_DLL_REFERENCES);
                var myProc = GetProcAddress(myD3d9, "_OpenDevTools");
                var lcuxProc = (uint)myProc - (uint)myD3d9 + lcuxD9d9;

                // Finally, call it.
                CreateRemoteThread(lcux, IntPtr.Zero, 0, (IntPtr)lcuxProc, (IntPtr)Convert.ToInt32(remote), 0, IntPtr.Zero);
            }
        }
    }
}
