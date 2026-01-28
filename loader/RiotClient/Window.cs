using System;
using System.Diagnostics;
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
                var sw = Stopwatch.StartNew();

                while (true)
                {
                    // Timeout after 1 minutes
                    if (sw.Elapsed.TotalMinutes > 1)
                    {
                        Log.Warn("Failed to find Riot Client window within timeout.");
                        break;
                    }

                    // Find main browser window
                    nint hwnd = FindWindow("Chrome_WidgetWin_1", "Riot Client");
                    GetWindowThreadProcessId(hwnd, out int wPid);

                    // It should be in the current process
                    if (hwnd != 0 && pid == wPid)
                    {
                        // Get exstyle to exclude the topmost splash window
                        nint exStyle = GetWindowLongPtr(hwnd, /*GWL_EXSTYLE*/ -20);
                        if ((exStyle & /*WS_EX_TOPMOST*/ 0x8) == 0)
                        {
                            Log.Info("Found Riot Client window: 0x{0:X}", hwnd);

                            // Set owner for message boxes
                            Utils.MessageBox.Owner = hwnd;

                            unsafe
                            {
                                _oldWndProc = GetWindowLongPtr(hwnd, /*GWL_WNDPROC*/ -4);

                                // Hook window proc
                                delegate* unmanaged[Stdcall]<IntPtr, uint, IntPtr, IntPtr, nint> ptr = &HookWndProc;
                                SetWindowLongPtr(hwnd, /*GWL_WNDPROC*/ -4, (nint)ptr);

                                SetWindowText(hwnd, "Riot Client ft. Pengu Loader");
                            }

                            break;
                        }
                    }

                    await Task.Delay(100);
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
                                _dbg?.ReloadPage();
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

        [LibraryImport("user32.dll", EntryPoint = "SetWindowTextW", StringMarshalling = StringMarshalling.Utf16)]
        private static partial int SetWindowText(nint hwnd, string lpString);

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