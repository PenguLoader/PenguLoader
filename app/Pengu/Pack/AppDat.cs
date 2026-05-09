using System.IO.Compression;
using System.Reflection;
using Pengu.Logging;

namespace Pengu.Pack;

/// <summary>
/// Reader for <c>app.dat</c>, the zipped SolidJS hub bundle that ships next
/// to <c>Pengu.exe</c>. Builds a <see cref="Dictionary{TKey, TValue}"/> of
/// entries on open so per-request lookup is O(1); the underlying
/// <see cref="ZipArchive"/> stays open for the life of the host so reads can
/// stream from it.
///
/// <para>Format: standard zip — produced by the MSBuild <c>ZipDirectory</c>
/// task in <c>Pengu.Windows.csproj</c> (Release config) from
/// <c>packages/hub/dist/</c>. Entry paths are forward-slashed and relative
/// to the dist root (<c>index.html</c>, <c>assets/index-...js</c>, etc.).</para>
///
/// <para>Thread-safety: <see cref="TryRead"/> serialises archive access
/// through a <c>lock</c>; <see cref="ZipArchiveEntry.Open"/> isn't safe
/// across concurrent readers from a single archive.</para>
/// </summary>
public sealed class AppDat : IDisposable
{
    private readonly ZipArchive _archive;
    private readonly Dictionary<string, ZipArchiveEntry> _index;
    private readonly object _gate = new();
    private bool _disposed;

    public string FilePath { get; }

    private AppDat(ZipArchive archive, Dictionary<string, ZipArchiveEntry> index, string path)
    {
        _archive = archive;
        _index = index;
        FilePath = path;
    }

    /// <summary>Open <c>app.dat</c> at <paramref name="path"/>. Throws if the
    /// file doesn't exist or isn't a valid zip.</summary>
    public static AppDat Open(string path)
    {
        var fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read);
        try
        {
            return BuildFromStream(fs, path);
        }
        catch
        {
            fs.Dispose();
            throw;
        }
    }

    /// <summary>
    /// Open <c>app.dat</c> from a managed assembly resource. Used by both
    /// heads since <c>app.dat</c> is embedded into the head's assembly at
    /// build time (via <c>EmbeddedResource</c>); this avoids shipping a
    /// separate file alongside the exe / inside the .app bundle.
    /// Returns null if the resource isn't present (Debug builds skip the
    /// hub bundle and rely on <c>--dev=URL</c> instead).
    /// </summary>
    public static AppDat? OpenEmbedded(Assembly assembly, string resourceName = "app.dat")
    {
        using var stream = assembly.GetManifestResourceStream(resourceName);
        if (stream is null)
        {
            Log.Debug("AppDat embedded resource '{0}' not present in {1}", resourceName, assembly.GetName().Name);
            return null;
        }
        // Copy into MemoryStream because ZipArchive needs a seekable stream
        // and we want to release the resource handle promptly.
        var ms = new MemoryStream();
        stream.CopyTo(ms);
        ms.Position = 0;
        try
        {
            return BuildFromStream(ms, $"<embedded:{resourceName}>");
        }
        catch
        {
            ms.Dispose();
            throw;
        }
    }

    private static AppDat BuildFromStream(Stream src, string identifier)
    {
        var archive = new ZipArchive(src, ZipArchiveMode.Read, leaveOpen: false);
        var index = new Dictionary<string, ZipArchiveEntry>(StringComparer.OrdinalIgnoreCase);
        foreach (var entry in archive.Entries)
        {
            if (string.IsNullOrEmpty(entry.FullName) || entry.FullName.EndsWith('/'))
                continue; // skip directory entries
            // Normalise forward-slashes in case the producer used '\' on Windows.
            var key = entry.FullName.Replace('\\', '/');
            index[key] = entry;
        }
        Log.Info("AppDat opened {0} ({1} entries)", identifier, index.Count);
        return new AppDat(archive, index, identifier);
    }

    /// <summary>Try to read <paramref name="path"/> from the pack. Returns
    /// false if the entry doesn't exist; on success populates the bytes and
    /// the MIME type derived from the entry's extension.</summary>
    public bool TryRead(string path, out byte[] content, out string mime)
    {
        if (!_index.TryGetValue(path, out var entry))
        {
            content = Array.Empty<byte>();
            mime = string.Empty;
            return false;
        }

        lock (_gate)
        {
            using var stream = entry.Open();
            using var ms = new MemoryStream(capacity: (int)Math.Min(int.MaxValue, entry.Length));
            stream.CopyTo(ms);
            content = ms.ToArray();
        }
        mime = MimeTypes.ForExtension(System.IO.Path.GetExtension(path));
        return true;
    }

    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;
        _archive.Dispose();
    }
}
