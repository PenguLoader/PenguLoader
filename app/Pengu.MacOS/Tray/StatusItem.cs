using AppKit;
using CoreGraphics;
using Foundation;
using Pengu;
using Pengu.Activation;
using Pengu.Bridge;
using Pengu.Logging;

namespace Pengu.MacOS.Tray;

/// <summary>
/// Menubar status-bar item. The macOS analogue of a system tray on Windows.
///
/// <para>Lives for the life of the daemon: the user closes the hub window
/// (which only orderOut's it, see <see cref="MacOSHost.CloseMainWindow"/>),
/// but the StatusItem stays in the menubar so the user can re-summon the
/// hub or toggle activation without re-launching Pengu.</para>
///
/// <para>The Activated checkbox is state-mirrored — it reads
/// <see cref="IActivationAction.IsActiveAsync"/> on every menu-open and
/// updates its <c>NSCellStateValue.On/Off</c> via a delegate hook that
/// AppKit fires before the menu draws.</para>
/// </summary>
internal sealed class StatusItem
{
    private readonly MacOSHost                 _host;
    private readonly ActivationActionRegistry  _registry;
    private readonly EventBus                  _bus;
    private readonly NSStatusItem              _item;
    private readonly NSMenuItem                _activatedItem;
    private readonly IDisposable               _busSubscription;

    public StatusItem(MacOSHost host, ActivationActionRegistry registry, EventBus bus)
    {
        _host     = host;
        _registry = registry;
        _bus      = bus;

        _item = NSStatusBar.SystemStatusBar.CreateStatusItem(NSStatusItemLength.Variable);

        // Tray icon: small PNG embedded as a managed resource. We render the
        // logo in its native colors (Template=false) — branded logo, not an
        // SF-Symbols-style monochrome glyph. Template=true is the macOS
        // convention for icons that should adopt the menubar's text color
        // (light/dark mode), which would re-tint the Pengu logo to plain
        // white/black and lose its colors.
        var icon = LoadEmbeddedImage("tray-icon.png");
        if (icon is not null)
        {
            icon.Size = new CGSize(18, 18); // standard menubar icon size
            if (_item.Button is { } btn) btn.Image = icon;
        }
        else if (_item.Button is { } btn)
        {
            btn.Title = "Pengu";
        }

        var menu = new NSMenu();

        // Header (disabled): "Pengu Loader vX.Y.Z"
        menu.AddItem(NewDisabled($"Pengu Loader v{AppEnv.AppVersion}"));
        menu.AddItem(NSMenuItem.SeparatorItem);

        menu.AddItem(NewItem("Open hub",                OnOpenHub,           keyEquivalent: ""));
        menu.AddItem(NSMenuItem.SeparatorItem);

        // The Activated toggle. State (checkmark) is refreshed in WillOpen.
        _activatedItem = NewItem("Activated",            OnToggleActive,      keyEquivalent: "");
        menu.AddItem(_activatedItem);
        menu.AddItem(NSMenuItem.SeparatorItem);

        menu.AddItem(NewItem("Open plugins folder",     OnOpenPluginsFolder, keyEquivalent: ""));
        menu.AddItem(NewItem("Reveal core.dylib",       OnRevealCoreDylib,   keyEquivalent: ""));
        menu.AddItem(NSMenuItem.SeparatorItem);

        menu.AddItem(NewItem("Quit",                    OnQuit,              keyEquivalent: "q"));

        // Refresh the activated checkbox right before AppKit draws the menu.
        menu.Delegate = new MenuDelegate(this);
        _item.Menu = menu;

        // Initial tooltip.
        UpdateTooltip();

        // Mirror the activated state in real time when activation changes
        // from elsewhere (e.g., the hub UI toggling it).
        _busSubscription = _bus.Subscribe(OnBusEvent);
    }

    public void Dispose()
    {
        _busSubscription.Dispose();
        NSStatusBar.SystemStatusBar.RemoveStatusItem(_item);
    }

    // ---------- menu actions ----------

    private void OnOpenHub(object? sender, EventArgs e) => _host.BringMainWindowToFront();

    private void OnToggleActive(object? sender, EventArgs e)
    {
        var action = _registry.Get(ActivationMode.Universal);
        if (action is null) return;
        // Fire-and-forget; bus event will refresh the menu state when done.
        _ = Task.Run(async () =>
        {
            try
            {
                bool current = await action.IsActiveAsync(CancellationToken.None).ConfigureAwait(false);
                var result = await action.SetActiveAsync(!current, CancellationToken.None).ConfigureAwait(false);
                if (!result.Ok)
                    Log.Warn("Tray toggle failed: {0}", result.Error ?? "(no error)");
            }
            catch (Exception ex)
            {
                Log.Error(ex, "Tray toggle threw");
            }
        });
    }

    private void OnOpenPluginsFolder(object? sender, EventArgs e)
    {
        var path = Path.Combine(_host.DataRoot, "plugins");
        Directory.CreateDirectory(path);
        NSWorkspace.SharedWorkspace.OpenUrl(NSUrl.FromFilename(path));
    }

    private void OnRevealCoreDylib(object? sender, EventArgs e)
    {
        var path = _host.CoreModulePath;
        if (!File.Exists(path))
        {
            // No core.dylib yet — open the parent (Resources/) so the user
            // can drop a build there.
            var parent = Path.GetDirectoryName(path);
            if (parent is not null && Directory.Exists(parent))
                NSWorkspace.SharedWorkspace.OpenUrl(NSUrl.FromFilename(parent));
            return;
        }
        NSWorkspace.SharedWorkspace.ActivateFileViewer([NSUrl.FromFilename(path)]);
    }

    private void OnQuit(object? sender, EventArgs e)
        => NSApplication.SharedApplication.Terminate(NSApplication.SharedApplication);

    // ---------- state + tooltip ----------

    private void RefreshActivatedItem()
    {
        var action = _registry.Get(ActivationMode.Universal);
        bool active = action is not null && action.IsActiveAsync(CancellationToken.None).GetAwaiter().GetResult();
        _activatedItem.State = active ? NSCellStateValue.On : NSCellStateValue.Off;
    }

    private void UpdateTooltip()
    {
        var action = _registry.Get(ActivationMode.Universal);
        bool active = action is not null && action.IsActiveAsync(CancellationToken.None).GetAwaiter().GetResult();
        var text = active ? "Pengu — Watching for League" : "Pengu — Off";
        if (_item.Button is { } btn) btn.ToolTip = text;
    }

    private void OnBusEvent(string name, string? payloadJson)
    {
        if (name == "activation:stateChanged")
        {
            // Marshal back to main thread; AppKit UI changes only.
            NSApplication.SharedApplication.InvokeOnMainThread(() =>
            {
                RefreshActivatedItem();
                UpdateTooltip();
            });
        }
    }

    // ---------- helpers ----------

    private static NSMenuItem NewItem(string title, EventHandler activated, string keyEquivalent)
    {
        var item = new NSMenuItem(title, keyEquivalent);
        item.Activated += activated;
        return item;
    }

    private static NSMenuItem NewDisabled(string title)
    {
        var item = new NSMenuItem(title) { Enabled = false };
        return item;
    }

    private static NSImage? LoadEmbeddedImage(string logicalName)
    {
        try
        {
            var asm = typeof(StatusItem).Assembly;
            using var stream = asm.GetManifestResourceStream(logicalName);
            if (stream is null) return null;
            using var ms = new MemoryStream();
            stream.CopyTo(ms);
            var data = NSData.FromArray(ms.ToArray());
            return new NSImage(data);
        }
        catch (Exception ex)
        {
            Log.Warn("Failed to load tray icon resource {0}: {1}", logicalName, ex.Message);
            return null;
        }
    }

    private sealed class MenuDelegate : NSMenuDelegate
    {
        private readonly StatusItem _owner;
        public MenuDelegate(StatusItem owner) { _owner = owner; }

        public override void MenuWillHighlightItem(NSMenu menu, NSMenuItem item) { }

        public override void MenuWillOpen(NSMenu menu)
        {
            try { _owner.RefreshActivatedItem(); }
            catch (Exception ex) { Log.Warn("Tray RefreshActivatedItem threw: {0}", ex.Message); }
        }
    }
}
