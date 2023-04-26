using System;
using System.IO;
using System.Linq;

namespace PenguLoader.Main
{
    static class Plugins
    {
        public static int CountEntries()
        {
            string pluginsDir = Config.PluginsDir;

            // All top-level JS files.
            int topLevelFiles = Directory.GetFiles(pluginsDir, "*.js", SearchOption.TopDirectoryOnly)
                .Count(path => FilterName(Path.GetFileName(path)));

            // All sub-folders with index.js file.
            int subdirectoryFiles = Directory.GetDirectories(pluginsDir)
                .Where(path => FilterName(Path.GetFileName(path)))
                .Sum(subdirectory => Directory.GetFiles(subdirectory, "index.js", SearchOption.TopDirectoryOnly).Length);

            return topLevelFiles + subdirectoryFiles;
        }

        // Ignore all file/dir names start with . or _
        static bool FilterName(string name) => !(name.StartsWith(".") || name.StartsWith("_"));
    }
}