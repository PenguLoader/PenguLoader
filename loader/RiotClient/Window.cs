using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Threading.Tasks;

namespace Pengu.Loader.RiotClient
{
    static partial class Window
    {
        static Debugger? _dbg;
        static nint _oldWndProc;

        public static void SetupWindow(Debugger dbg)
        {
            _dbg = dbg;

            Task.Run(async () =>
            {
                int pid = GetCurrentProcessId();

                while (true)
                {
                    // Find main browser window
                    nint hwnd = FindWindow("Chrome_WidgetWin_1", "Riot Client");
                    GetWindowThreadProcessId(hwnd, out int wPid);

                    // It should be in the current process
                    if (hwnd != 0 && pid == wPid)
                    {
                        // Set owner for message boxes
                        Utils.MessageBox.Owner = hwnd;

                        unsafe
                        {
                            _oldWndProc = GetWindowLongPtr(hwnd, -4);

                            // Hook window proc
                            delegate* unmanaged[Stdcall]<IntPtr, uint, IntPtr, IntPtr, nint> ptr = &HookWndProc;
                            SetWindowLongPtr(hwnd, -4, (nint)ptr);
                        }

                        break;
                    }

                    await Task.Delay(50);
                }
            });
        }

        [UnmanagedCallersOnly(CallConvs = [typeof(CallConvStdcall)])]
        static nint HookWndProc(IntPtr hwnd, uint msg, IntPtr wparam, IntPtr lparam)
        {
            if (msg == 0x0100) // WM_KEYDOWN
            {
                if ((HIWORD(lparam) & 0x4000) == 0)     // no-repeat
                {
                    if (wparam == 0x7B)                 // F12
                    {
                        _dbg?.OpenRemoteDevTools();
                    }
                    else if (GetKeyState(0x11) < 0      // CTRL
                        && GetKeyState(0x10) < 0)       // SHIFT
                    {
                        switch (wparam)
                        {
                            case 'I':
                                _dbg?.OpenRemoteDevTools();
                                return 0;

                            case 'R':
                                _dbg?.DevTools.ReloadPage();
                                return 0;
                        }
                    }
                }
            }

            return CallWindowProc(_oldWndProc, hwnd, msg, wparam, lparam);
        }

        [LibraryImport("user32.dll", EntryPoint = "FindWindowA", StringMarshalling = StringMarshalling.Utf8)]
        private static partial nint FindWindow(string klass, string name);

        [LibraryImport("user32.dll", EntryPoint = "CallWindowProcW")]
        private static partial nint CallWindowProc(nint prev, nint hwnd, uint msg, nint wp, nint lp);

        [LibraryImport("user32.dll", EntryPoint = "GetWindowLongPtrW")]
        private static partial nint GetWindowLongPtr(nint hwnd, int nIndex);

        [LibraryImport("user32.dll", EntryPoint = "SetWindowLongPtrW")]
        private static partial nint SetWindowLongPtr(nint hwnd, int nIndex, nint dwNewLong);

        [LibraryImport("user32.dll", SetLastError = true)]
        private static partial uint GetWindowThreadProcessId(nint hWnd, out int pid);

        [LibraryImport("kernel32.dll")]
        private static partial int GetCurrentProcessId();

        [LibraryImport("user32.dll")]
        private static partial short GetKeyState(int key);

        private static ushort HIWORD(IntPtr value)
            => unchecked((ushort)((((long)(value)) >> 16) & 0xFFFF));
    }
}