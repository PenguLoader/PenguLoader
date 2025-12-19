using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Threading.Tasks;

namespace Pengu.Loader.RiotClient
{
    class JsBridge
    {
        public class FsError(string code, string message) : Exception(message)
        {
            public readonly string Code = code;
        }

        public JsBridge()
        {
        }

        public Task<JsValue> FsExsits(JsonElement[] args)
        {
            if (args.Length != 1)
                throw new FsError("EINVAL", "Invalid number of arguments");

            var path = args[0].GetString();
            if (string.IsNullOrEmpty(path))
                throw new FsError("EINVAL", "Path cannot be null");

            var val = File.Exists(path);
            var ret = JsValue.Boolean(val);

            return Task.FromResult(ret);
        }

        public Task<JsValue> FsStat(JsonElement[] args)
        {
            if (args.Length != 1)
                throw new FsError("EINVAL", "Invalid number of arguments");

            var path = args[0].GetString();
            if (string.IsNullOrEmpty(path))
                throw new FsError("EINVAL", "Path cannot be null");

            var attr = new FileInfo(path);

            var ret = JsValue.Object(new Dictionary<string, JsValue>
            {
                { "size", JsValue.Number(attr.Length) },
                { "isFile", JsValue.Boolean(attr.Exists && !attr.Attributes.HasFlag(FileAttributes.Directory)) },
                { "isDirectory", JsValue.Boolean(attr.Exists && attr.Attributes.HasFlag(FileAttributes.Directory)) },
            });

            return Task.FromResult(ret);
        }

        public Task<JsValue> FsReaddir(JsonElement[] args)
        {
            if (args.Length != 1)
                throw new FsError("EINVAL", "Invalid number of arguments");
            var path = args[0].GetString();
            if (string.IsNullOrEmpty(path))
                throw new FsError("EINVAL", "Path cannot be null");
            string[] entries;
            try
            {
                entries = Directory.GetFileSystemEntries(path);
            }
            catch (Exception ex)
            {
                throw new FsError("EIO", $"Failed to read directory: {ex.Message}");
            }
            var jsEntries = entries.Select(e => JsValue.String(Path.GetFileName(e))).ToList();
            var ret = JsValue.Array(jsEntries);
            return Task.FromResult(ret);
        }

        public Task<JsValue> FsMkdir(JsonElement[] args)
        {
            if (args.Length != 1)
                throw new FsError("EINVAL", "Invalid number of arguments");
            var path = args[0].GetString();
            if (string.IsNullOrEmpty(path))
                throw new FsError("EINVAL", "Path cannot be null");
            try
            {
                Directory.CreateDirectory(path);
            }
            catch (Exception ex)
            {
                throw new FsError("EIO", $"Failed to create directory: {ex.Message}");
            }
            return Task.FromResult(JsValue.Null);
        }

        public Task<JsValue> FsRemove(JsonElement[] args)
        {
            if (args.Length < 1)
                throw new FsError("EINVAL", "Invalid number of arguments");

            var path = args[0].GetString();
            if (string.IsNullOrEmpty(path))
                throw new FsError("EINVAL", "Path cannot be null");

            bool silent = false;
            bool recursive = false;
            
            if (args.Length >= 2)
            {
                var options = args[1];
                silent = options.GetProperty("silent").GetBoolean();
                recursive = options.GetProperty("recursive").GetBoolean();
            }

            try
            {
                var attrs = File.GetAttributes(path);
                if (attrs.HasFlag(FileAttributes.Directory))
                {
                    Directory.Delete(path, recursive);
                    return Task.FromResult(JsValue.True);
                }
                else
                {
                    File.Delete(path);
                    return Task.FromResult(JsValue.True);
                }
            }
            catch (FileNotFoundException)
            {
                if (!silent)
                    throw new FsError("ENOENT", "No such file or directory");
            }
            catch (Exception ex)
            {
                if (!silent)
                    throw new FsError("EIO", ex.Message);
            }

            return Task.FromResult(JsValue.False);
        }
    }
}