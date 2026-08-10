using Pengu.Logging;

namespace Pengu.Plugins;

/// <summary>
/// Walks the plugins directory and produces the flat list the hub displays
/// and the C++ core's renderer ultimately loads.
///
/// <para>Recognises three entry layouts (matches the C++ core's
/// <c>get_plugin_entries</c> in <c>core/src/renderer/renderer.cc</c>):</para>
/// <list type="bullet">
///   <item><description><c>name.js</c> — top-level single-file plugin.</description></item>
///   <item><description><c>name/index.js</c> — folder plugin.</description></item>
///   <item><description><c>@author/name/index.js</c> — author-namespaced.</description></item>
/// </list>
///
/// <para>Names beginning with <c>_</c> or <c>.</c> are skipped. Subfolders
/// of an <c>@author</c> folder follow the same skip rule. The vestigial
/// v0.6 <c>@default/*</c> namespace is also skipped (the renderer drops it
/// in <c>preload/loader.ts</c>).</para>
///
/// <para>Legacy <c>.js_</c> / <c>index.js_</c> entries (v1.1.6's
/// rename-to-disable mechanism) are recognised so old installs surface
/// their plugins. The canonical path stored on <see cref="PluginInfo"/>
/// has the trailing <c>_</c> stripped, so the FNV-1a hash matches what
/// v1.2.0's renderer computes from the URL. The <see cref="PluginInfo.Enabled"/>
/// flag treats a legacy entry as disabled regardless of the hash csv —
/// when the user toggles such a plugin on,
/// <see cref="Pengu.Api.PluginsApi.ToggleEnabled"/> renames the file back
/// to <c>.js</c> / <c>index.js</c>, after which it follows the steady-state
/// hash-csv convention.</para>
/// </summary>
public sealed class PluginDiscovery
{
    /// <summary>
    /// Hand-rolled equivalent of <c>@{tag}\s+(.+)</c> with <c>IgnoreCase</c>:
    /// find the first case-insensitive <c>@tag</c>, skip the whitespace run
    /// after it, take the rest of that line.
    ///
    /// <para>Not a <see cref="System.Text.RegularExpressions.Regex"/> because
    /// four literal tags don't pay for one: referencing the assembly at all
    /// costs 664 KB in the AOT image (the interpreter, parser and character
    /// class tables, plus the corelib formatting paths only they root). Note
    /// that <c>RegexOptions.Compiled</c> was a silent no-op here anyway —
    /// NativeAOT has no runtime codegen, so it fell back to the interpreter
    /// and we paid for the option without getting it.</para>
    ///
    /// <para>Matches the regex on the edge cases that matter: <c>\s+</c> may
    /// span newlines (so a tag whose value starts on the next line still
    /// matches), while <c>.</c> excludes <c>\n</c> (so the value stops at
    /// end-of-line). Both quantifiers require at least one character, hence
    /// the two <c>continue</c>s — a bare <c>@name</c> with nothing after it
    /// is not a match, and scanning resumes at the next <c>@</c>.</para>
    ///
    /// <para>One deliberate divergence. For a value-less tag followed by two
    /// or more whitespace characters ending in something other than <c>\n</c>
    /// (<c>"@name  \r\n"</c>, <c>"@name\t\t"</c>), the regex backtracks
    /// <c>\s+</c> and lets <c>(.+)</c> capture a stray <c>\r</c> or space,
    /// returning <c>""</c> after the trim. This returns <c>null</c>, which is
    /// what the callers want — an absent value, not a present empty one, so
    /// the field is omitted from the bridge payload rather than serialized as
    /// <c>""</c>. Verified equivalent otherwise across 164 tag/input pairs.</para>
    ///
    /// <para>Inherited quirk, present in the regex too and left alone: a
    /// value-less tag inside a JSDoc block swallows the following line,
    /// because the leading <c>" * "</c> is whitespace up to the <c>*</c>. So
    /// <c>"/** \n * @name\n * @author bob\n */"</c> yields the name
    /// <c>"* @author bob"</c>.</para>
    /// </summary>
    private static string? FindTag(string content, string tag)
    {
        int i = 0;
        while ((i = content.IndexOf('@', i)) >= 0)
        {
            i++;
            if (string.Compare(content, i, tag, 0, tag.Length, StringComparison.OrdinalIgnoreCase) != 0)
                continue;

            int p = i + tag.Length;
            int wsStart = p;
            while (p < content.Length && char.IsWhiteSpace(content[p])) p++;
            if (p == wsStart) continue;

            int end = content.IndexOf('\n', p);
            if (end < 0) end = content.Length;
            if (end == p) continue;

            return content[p..end].Trim();
        }
        return null;
    }

    public IReadOnlyList<PluginInfo> List(string pluginsDir, HashSet<uint> disabledHashes)
    {
        var results = new List<PluginInfo>();
        if (!Directory.Exists(pluginsDir))
        {
            Log.Debug("PluginDiscovery: dir does not exist: {0}", pluginsDir);
            return results;
        }

        // Normalise so substring stripping in MakeRelative works on either OS.
        var normalisedRoot = pluginsDir.Replace('\\', '/').TrimEnd('/');

        foreach (var entry in Directory.EnumerateFileSystemEntries(pluginsDir))
        {
            var name = Path.GetFileName(entry);
            if (!IsAllowedName(name)) continue;

            if (Directory.Exists(entry))
            {
                if (name.StartsWith('@'))
                {
                    if (string.Equals(name, "@default", StringComparison.OrdinalIgnoreCase))
                        continue; // v0.6 vestigial namespace, never surface

                    foreach (var sub in Directory.EnumerateDirectories(entry))
                    {
                        var subName = Path.GetFileName(sub);
                        if (!IsAllowedName(subName)) continue;
                        if (TryGetIndex(sub, out var subEntry))
                        {
                            var displayName = $"{name}/{subName}";
                            results.Add(Build(displayName, subEntry, normalisedRoot, disabledHashes));
                        }
                    }
                }
                else if (TryGetIndex(entry, out var indexPath))
                {
                    results.Add(Build(name, indexPath, normalisedRoot, disabledHashes));
                }
            }
            else if (IsTopLevelJs(entry))
            {
                // Skip legacy .js_ when its live .js peer exists, so we don't
                // surface two cards for the same canonical plugin. Mirrors
                // TryGetIndex's "live wins over legacy" priority for the
                // folder case.
                if (entry.EndsWith('_'))
                {
                    var live = entry[..^1];
                    if (File.Exists(live)) continue;
                }
                var displayName = Path.GetFileNameWithoutExtension(name);
                results.Add(Build(displayName, entry, normalisedRoot, disabledHashes));
            }
        }

        return results;
    }

    // -------- helpers --------

    private static bool IsAllowedName(string? name) =>
        !string.IsNullOrEmpty(name) && name[0] != '_' && name[0] != '.';

    private static bool IsTopLevelJs(string path)
    {
        if (path.EndsWith(".js", StringComparison.OrdinalIgnoreCase) && File.Exists(path)) return true;
        // legacy disabled rename: foo.js_
        if (path.EndsWith(".js_", StringComparison.Ordinal) && File.Exists(path)) return true;
        return false;
    }

    /// <summary>True if <paramref name="dir"/> contains <c>index.js</c> (or
    /// the v1.1.6 legacy <c>index.js_</c>); writes the actual entry path to
    /// <paramref name="entryPath"/>.</summary>
    private static bool TryGetIndex(string dir, out string entryPath)
    {
        var indexJs = Path.Combine(dir, "index.js");
        if (File.Exists(indexJs)) { entryPath = indexJs; return true; }

        var indexJsLegacy = indexJs + "_";
        if (File.Exists(indexJsLegacy)) { entryPath = indexJsLegacy; return true; }

        entryPath = string.Empty;
        return false;
    }

    private static PluginInfo Build(string displayName, string entryPath, string normalisedRoot, HashSet<uint> disabledHashes)
    {
        // Forward-slashed relative path with any trailing legacy `_` stripped.
        // This is the canonical form the C++ core's plugin URL uses, so the
        // FNV-1a hash matches what the renderer-side disabled-list filter
        // computes from the URL.
        var rel = entryPath.Replace('\\', '/');
        if (rel.StartsWith(normalisedRoot, StringComparison.OrdinalIgnoreCase))
            rel = rel[normalisedRoot.Length..].TrimStart('/');

        // v1.1.6 legacy: trailing `_` on the file (the rename-to-disable
        // convention). Treated as disabled here so existing installs see
        // their plugins as off; PluginsApi.ToggleEnabled renames back to
        // the live form on the user's first toggle, after which the
        // hash-csv convention is the only signal.
        bool isLegacyDisabled = rel.EndsWith('_');
        if (isLegacyDisabled) rel = rel[..^1];

        var hash = Fnv1a.Hash(rel.ToLowerInvariant());
        var (jsdocName, description, author, link) = ReadJsDoc(entryPath);

        // @name in the entry's JSDoc wins over the filename / folder name when
        // present. Lets plugin authors choose a display title independent of
        // their on-disk slug. Whitespace-only @name is ignored.
        var finalName = !string.IsNullOrWhiteSpace(jsdocName) ? jsdocName! : displayName;

        return new PluginInfo(
            Name: finalName,
            Path: rel,
            Hash: hash,
            Author: author,
            Description: description,
            Link: link,
            Enabled: !isLegacyDisabled && !disabledHashes.Contains(hash));
    }

    private static (string? name, string? description, string? author, string? link) ReadJsDoc(string path)
    {
        try
        {
            // Bound the read so we don't pull in a 50 MB plugin file just to
            // grep for tags. Real JSDoc lives at the very top of the file.
            string content;
            using (var fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read))
            using (var sr = new StreamReader(fs))
            {
                var buf = new char[8192];
                int read = sr.Read(buf, 0, buf.Length);
                content = new string(buf, 0, read);
            }

            var name        = FindTag(content, "name");
            var description = FindTag(content, "description");
            var author      = FindTag(content, "author");
            var link        = FindTag(content, "link");

            // v1.1.6 convention: bare authors get an `@` prefix; tagged
            // authors (containing `#`) keep their full handle.
            if (author is { Length: > 0 } && !author.Contains('#') && !author.StartsWith('@'))
                author = "@" + author;

            // Hub renders the link via Shell.OpenLink which already requires
            // https://, but mirroring the original validation here keeps
            // malformed YAML / typos from polluting the UI.
            if (link is { Length: > 0 } && !link.StartsWith("https://", StringComparison.OrdinalIgnoreCase))
                link = null;

            return (name, description, author, link);
        }
        catch
        {
            return (null, null, null, null);
        }
    }
}
