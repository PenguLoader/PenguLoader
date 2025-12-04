using System;
using System.Runtime.InteropServices;

namespace Pengu.Loader.Utils
{
    internal class MessageBox
    {
        const string Title = "Riot Client";
        public static IntPtr Owner { get; set; }

        public static void Show(string message, bool warning = false)
        {
            int flags = 0x0;
            if (warning) flags |= 0x30;

            MsgBox(Owner, message, Title, flags);
        }

        [DllImport("user32.dll", EntryPoint = "MessageBoxW", CharSet = CharSet.Unicode)]
        static extern int MsgBox(IntPtr hwnd, string msg, string title, int flags);
    }
}