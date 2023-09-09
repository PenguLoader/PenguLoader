﻿using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;

namespace PenguLoader.Main
{
    public static class Plugins
    {
        public static List<PluginInfo> All()
        {
            var pluginsDir = Config.PluginsDir;
            var plugins = new List<PluginInfo>();

            // sub-folders plugins.
            foreach (var dir in Directory.GetDirectories(pluginsDir))
            {
                var dirName = Path.GetFileName(dir);
                if (!FilterName(dirName)) continue;
                var disabled = false;
                string indexPath;

                if (File.Exists(indexPath = Path.Combine(dir, "index.js")) ||
                    (disabled = File.Exists(indexPath = Path.Combine(dir, "index.js_"))))
                    AddPlugin(dirName, indexPath, !disabled);
            }

            // top-level plugins.
            foreach (var path in Directory.GetFiles(pluginsDir, "*.*", SearchOption.TopDirectoryOnly))
            {
                var fileName = Path.GetFileName(path);
                var disabled = false;

                if (FilterName(fileName) &&
                    (fileName.EndsWith(".js") || (disabled = fileName.EndsWith(".js_"))))
                    AddPlugin(fileName.TrimEnd('_'), path, !disabled);
            }

            return plugins;

            void AddPlugin(string name, string path, bool enabled)
            {
                var plugin = new PluginInfo
                {
                    Name = name,
                    Path = path,
                    Enabled = enabled
                };

                ParsePluginEntry(path, plugin);
                plugins.Add(plugin);
            }
        }

        public static void Toggle(PluginInfo plugin)
        {
            var path = plugin.Path;

            if (File.Exists(path))
            {
                if (path.EndsWith(".js_"))
                {
                    File.Move(path, plugin.Path = path.TrimEnd('_'));
                    plugin.Enabled = true;
                }
                else if (path.EndsWith(".js"))
                {
                    File.Move(path, plugin.Path = path + '_');
                    plugin.Enabled = false;
                }
            }
        }

        // Ignore name starts with . or _
        private static bool FilterName(string name)
        {
            return !(name.StartsWith(".") || name.StartsWith("_"));
        }

        // Get @author and @link.
        private static void ParsePluginEntry(string path, PluginInfo plugin)
        {
            if (!File.Exists(path)) return;
            var content = File.ReadAllText(path, Encoding.UTF8);

            var author = GetTagValue(content, "author");
            if (!string.IsNullOrEmpty(author))
                plugin.Author = author.Contains("#") ? author : '@' + author;

            var link = GetTagValue(content, "link");
            if (link.StartsWith("http://") || link.StartsWith("https://"))
                plugin.Link = link;
        }

        // Parse @tag in jsdoc
        private static string GetTagValue(string jsdoc, string tagName)
        {
            var pattern = $"@{tagName}\\s+(.+)";
            var match = Regex.Match(jsdoc, pattern);
            return match.Success ? match.Groups[1].Value.Trim() : string.Empty;
        }

        public class PluginInfo
        {
            public string Author = string.Empty;
            public bool Enabled;
            public string Link = string.Empty;
            public string Name;
            public string Path;
        }
    }
}