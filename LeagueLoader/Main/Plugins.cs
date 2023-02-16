using System;
using System.IO;

namespace LeagueLoader.Main
{
    static class Plugins
    {
        public static int CountEntries()
        {
            int count = 0;
            var pluginsDir = Config.PluginsDir;

            foreach (var path in Directory.GetFiles(pluginsDir, "*.js", SearchOption.TopDirectoryOnly))
            {
                var name = Path.GetFileName(path);
                if (!name.StartsWith(".") && !name.StartsWith("_"))
                    count++;
            }

            foreach (string subdirectory in Directory.GetDirectories(pluginsDir))
            {
                var files = Directory.GetFiles(subdirectory, "index.js", SearchOption.TopDirectoryOnly);
                count += files.Length;
            }

            return count;
        }
    }
}