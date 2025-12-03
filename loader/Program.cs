using System;
using System.Runtime.InteropServices;

namespace Pengu.Loader
{
    static partial class Program
    {
        [UnmanagedCallersOnly]
        public static int Entry()
        {
            MessageBoxW(IntPtr.Zero, "Hello from Pengu Loader", "Pengu Loader", 0);
            return 0;
        }

        // MessageBox
        [LibraryImport("user32.dll", SetLastError = true, StringMarshalling = StringMarshalling.Utf16)]
        private static partial int MessageBoxW(IntPtr hWnd, string lpText, string lpCaption, uint uType);
    }
}