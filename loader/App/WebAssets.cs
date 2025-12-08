using System;
using System.IO;
using System.Reflection;

namespace Pengu.Loader.App
{
    static class WebAssets
    {
        static Assembly _asm = typeof(WebAssets).Assembly;
        static readonly string _prefix = /*_asm.GetName().Name!*/"WebAssets";

        public static bool TryGetFile(string path, out Stream? stream, out string? contentType)
        {
            if (path == "/") path = "/index.html";
            var fullName = _prefix + path;

            stream = _asm.GetManifestResourceStream(fullName);
            if (stream == null)
            {
                contentType = null;
                return false;
            }

            contentType = GuessContentType(path);
            return true;
        }

        private static string GuessContentType(string path)
        {
            var ext = Path.GetExtension(path).ToLower();
            if (MimeTypes.TryGetMimeType(ext, out var mime))
            {
                return mime;
            }

            return "application/octet-stream";
        }
    }
}