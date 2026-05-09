using AppKit;
using CoreGraphics;
using Foundation;
using Pengu.Bridge;
using Pengu.Logging;
using Pengu.Pack;
using WebKit;

namespace Pengu.MacOS.Browser;

/// <summary>
/// One <see cref="WKWebView"/> hosted as a window's contentView. Counterpart
/// of <c>Pengu.Windows.Browser.Browser</c>; implements <see cref="IBrowserHost"/>
/// so the cross-platform <see cref="JsBridge"/> can drive it identically on
/// both platforms.
///
/// <para>The webview's <c>WKWebViewConfiguration</c> registers a
/// <see cref="HubAssetSchemeHandler"/> for <c>app://</c> when an
/// <see cref="AppDat"/> is supplied (Release / packed builds). Dev mode
/// (<see cref="AppEnv.DevUrl"/> set) skips the handler and navigates straight
/// to the Vite dev server URL.</para>
/// </summary>
internal sealed class WkWebViewHost : NSObject, IBrowserHost, IWKScriptMessageHandler
{
    /// <summary>JS-side handler name. Renderer posts via
    /// <c>window.webkit.messageHandlers.pengu.postMessage(jsonString)</c>.</summary>
    private const string MessageHandlerName = "pengu";

    private readonly WKWebView          _webView;
    private readonly NavigationLogger   _navDelegate;
    private readonly JsDialogUIDelegate _uiDelegate;

    public event Action<string>? WebMessageReceivedAsJson;

    /// <summary>The view to mount as the window's contentView.</summary>
    public NSView View => _webView;

    public WkWebViewHost(AppDat? packedAppDat)
    {
        var config = new WKWebViewConfiguration();

        // Custom URL scheme handler for the packed bundle. Always registered
        // even when packedAppDat is null — handler returns 404 for every
        // request in that case, but the registration is what stops WKWebView
        // from escalating an unknown scheme to NSWorkspace's "what app
        // handles app://?" system prompt.
        var handler = new HubAssetSchemeHandler(packedAppDat);
        config.SetUrlSchemeHandler(handler, "app");

        // User content controller carries (a) the chrome.webview polyfill
        // (b) the cross-platform JsBridgeShim injected by JsBridge.InjectScript,
        // and (c) our message handler that catches renderer→host posts.
        var ucc = new WKUserContentController();
        ucc.AddScriptMessageHandler(this, MessageHandlerName);
        config.UserContentController = ucc;

        // Inject a polyfill that maps WebView2's window.chrome.webview API to
        // WKWebView's native primitives. The cross-platform JsBridgeShim is
        // written for chrome.webview (postMessage + 'message' addEventListener);
        // by polyfilling here, the same shim runs unchanged on macOS. Must be
        // added BEFORE JsBridge.InjectScript so it loads first at
        // document-start.
        var polyfill = new WKUserScript(
            (NSString)ChromeWebViewPolyfill,
            WKUserScriptInjectionTime.AtDocumentStart,
            isForMainFrameOnly: false);
        ucc.AddUserScript(polyfill);

        // Drag-region polyfill: WKWebView ignores `app-region: drag` CSS
        // (WebView2-specific), so on mousedown we walk up the DOM to find an
        // ancestor with computed app-region: drag and call
        // pengu.host.startDragging() — host synthesizes an NSEvent and
        // initiates a native window drag. Hub continues to use the same CSS
        // it uses on Windows.
        var dragShim = new WKUserScript(
            (NSString)DragRegionPolyfill,
            WKUserScriptInjectionTime.AtDocumentStart,
            isForMainFrameOnly: false);
        ucc.AddUserScript(dragShim);

        // DevTools (Web Inspector) on in --dev mode, off otherwise. WKWebView
        // exposes this via Configuration.Preferences.SetValueForKey since
        // the public Inspectable property is macOS 13.3+.
        if (AppEnv.DevUrl is not null)
        {
            using var trueObj = NSNumber.FromBoolean(true);
            config.Preferences.SetValueForKey(trueObj, (NSString)"developerExtrasEnabled");
        }

        _webView = new WKWebView(CGRect.Empty, config);
        _webView.AutoresizingMask = NSViewResizingMask.WidthSizable | NSViewResizingMask.HeightSizable;

        _navDelegate = new NavigationLogger();
        _webView.NavigationDelegate = _navDelegate;

        // Bridge JS alert/confirm/prompt to NSAlert. Without this, WKWebView
        // silently drops window.alert() — unlike WebView2 which renders it
        // natively. The hub uses alert() for activation errors etc., so
        // wiring this here is what makes hub's existing UX work on macOS.
        _uiDelegate = new JsDialogUIDelegate();
        _webView.UIDelegate = _uiDelegate;
    }

    public void Navigate(string url)
    {
        var nsUrl = NSUrl.FromString(url)
            ?? throw new ArgumentException($"Invalid URL: {url}", nameof(url));
        _webView.LoadRequest(new NSUrlRequest(nsUrl));
        Log.Info("WKWebView navigating to {0}", url);
    }

    public void PostWebMessageAsJson(string json)
    {
        // Bridge protocol: dispatch a CustomEvent on window so the JS shim's
        // listener picks up replies and pushes uniformly. Mirror of the
        // chrome.webview.message path on Windows.
        var script = $"window.dispatchEvent(new CustomEvent('pengu:message',{{detail:{json}}}));";
        _webView.EvaluateJavaScript(script, (result, error) =>
        {
            if (error is not null)
                Log.Warn("PostWebMessageAsJson failed: {0}", error.LocalizedDescription);
        });
    }

    public void AddScriptToExecuteOnDocumentCreated(string script)
    {
        var userScript = new WKUserScript(
            (NSString)script,
            WKUserScriptInjectionTime.AtDocumentStart,
            isForMainFrameOnly: false);
        _webView.Configuration.UserContentController.AddUserScript(userScript);
    }

    [Export("userContentController:didReceiveScriptMessage:")]
    public void DidReceiveScriptMessage(WKUserContentController userContentController, WKScriptMessage message)
    {
        if (message.Name != MessageHandlerName) return;
        // JS side posts JSON.stringify({...}); message.Body comes back as a
        // managed string (NSString -> .NET string) thanks to AppKit bindings.
        var json = message.Body?.ToString();
        if (!string.IsNullOrEmpty(json))
            WebMessageReceivedAsJson?.Invoke(json);
    }

    /// <summary>
    /// Mousedown polyfill that bridges <c>.app-drag</c> regions to
    /// <c>pengu.host.startDragging()</c>. WKWebView's CSS engine doesn't
    /// recognize the <c>-webkit-app-region</c> property at all (it's a
    /// WebView2/Edge-specific feature), so we can't rely on
    /// <c>getComputedStyle</c>. Instead, key off the hub's existing
    /// <c>.app-drag</c> class plus tag-name/role checks for interactive
    /// children — matches the intent of the CSS rule
    /// <c>.app-drag :where(button, a, input, [role='button'], .app-no-drag) { app-region: no-drag }</c>.
    /// </summary>
    private const string DragRegionPolyfill =
        """
        (function () {
          function isInteractive(el) {
            if (!el || !el.tagName) return false;
            var t = el.tagName;
            if (t === 'BUTTON' || t === 'A' || t === 'INPUT' || t === 'SELECT' || t === 'TEXTAREA') return true;
            if (el.getAttribute && el.getAttribute('role') === 'button') return true;
            if (el.classList && el.classList.contains('app-no-drag')) return true;
            return false;
          }
          document.addEventListener('mousedown', function (e) {
            if (e.button !== 0) return;
            var el = e.target;
            var inDrag = false;
            while (el && el !== document.documentElement) {
              // If anything interactive is on the path between the click
              // target and a drag ancestor, don't drag — the CSS no-drag
              // override would have applied for these.
              if (isInteractive(el)) return;
              if (el.classList && el.classList.contains('app-drag')) { inDrag = true; break; }
              el = el.parentElement;
            }
            if (!inDrag) return;
            if (window.pengu && window.pengu.host && window.pengu.host.startDragging) {
              window.pengu.host.startDragging();
            }
          }, true /* capture phase: beat WKWebView's own drag handling */);
        })();
        """;

    /// <summary>
    /// Polyfill that exposes <c>window.chrome.webview</c> on WKWebView,
    /// matching the shape <see cref="JsBridgeShim"/> expects on Windows.
    /// Maps <c>postMessage(obj)</c> → native message handler, and
    /// <c>addEventListener('message', cb)</c> → CustomEvent <c>pengu:message</c>
    /// dispatched by <see cref="PostWebMessageAsJson"/>.
    /// </summary>
    private const string ChromeWebViewPolyfill =
        """
        (function () {
          if (window.chrome && window.chrome.webview) return;
          if (!window.chrome) window.chrome = {};
          var listeners = [];
          window.chrome.webview = {
            postMessage: function (obj) {
              window.webkit.messageHandlers.pengu.postMessage(JSON.stringify(obj));
            },
            addEventListener: function (name, handler) {
              if (name === 'message') listeners.push(handler);
            },
            removeEventListener: function (name, handler) {
              if (name === 'message') {
                var i = listeners.indexOf(handler);
                if (i >= 0) listeners.splice(i, 1);
              }
            }
          };
          window.addEventListener('pengu:message', function (e) {
            for (var i = 0; i < listeners.length; i++) {
              try { listeners[i]({ data: e.detail }); }
              catch (err) { console.error('pengu bridge listener threw', err); }
            }
          });
        })();
        """;

    /// <summary>
    /// WKUIDelegate that bridges JS dialog primitives (<c>window.alert</c>,
    /// <c>window.confirm</c>, <c>window.prompt</c>) to native NSAlert. Without
    /// these implementations, WKWebView silently drops the calls — different
    /// from WebView2 which renders them natively. The hub already calls
    /// <c>alert(...)</c> for activation failures etc.; implementing this
    /// makes those calls work without any hub-side changes.
    /// </summary>
    private sealed class JsDialogUIDelegate : WKUIDelegate
    {
        public override void RunJavaScriptAlertPanel(
            WKWebView webView, string message, WKFrameInfo frame, Action completionHandler)
        {
            try
            {
                using var alert = new NSAlert
                {
                    MessageText     = "Pengu",
                    InformativeText = message ?? string.Empty,
                    AlertStyle      = NSAlertStyle.Informational,
                };
                alert.AddButton("OK");
                alert.RunModal();
            }
            finally
            {
                // MUST always call the completion handler — JS execution is
                // suspended until we do.
                completionHandler();
            }
        }

        public override void RunJavaScriptConfirmPanel(
            WKWebView webView, string message, WKFrameInfo frame, Action<bool> completionHandler)
        {
            bool ok = false;
            try
            {
                using var alert = new NSAlert
                {
                    MessageText     = "Pengu",
                    InformativeText = message ?? string.Empty,
                    AlertStyle      = NSAlertStyle.Informational,
                };
                alert.AddButton("OK");
                alert.AddButton("Cancel");
                ok = alert.RunModal() == (long)NSAlertButtonReturn.First;
            }
            finally
            {
                completionHandler(ok);
            }
        }

        // Apple deprecated the synchronous text-input panel WKUIDelegate
        // method in favor of an async (completion-handler-based) sibling.
        // The deprecated form still works on macOS 12+ and is what the .NET
        // binding exposes; explicit [Obsolete] silences CS0672.
        [Obsolete("Implements the deprecated WKUIDelegate text-input override; intentional on macOS 12-14.")]
        public override void RunJavaScriptTextInputPanel(
            WKWebView webView, string prompt, string? defaultText, WKFrameInfo frame,
            Action<string> completionHandler)
        {
            // The bound API surfaces a non-nullable result, but the underlying
            // semantics in WebKit allow signaling cancel via empty/null. We
            // pass empty string on cancel (the common JS prompt() convention
            // is to treat falsy as cancellation upstream).
            string result = string.Empty;
            try
            {
                using var alert = new NSAlert
                {
                    MessageText     = "Pengu",
                    InformativeText = prompt ?? string.Empty,
                    AlertStyle      = NSAlertStyle.Informational,
                };
                using var input = new NSTextField(new CGRect(0, 0, 280, 22))
                {
                    StringValue = defaultText ?? string.Empty,
                };
                alert.AccessoryView = input;
                alert.AddButton("OK");
                alert.AddButton("Cancel");
                if (alert.RunModal() == (long)NSAlertButtonReturn.First)
                    result = input.StringValue ?? string.Empty;
            }
            finally
            {
                completionHandler(result);
            }
        }
    }

    private sealed class NavigationLogger : WKNavigationDelegate
    {
        public override void DidFinishNavigation(WKWebView webView, WKNavigation navigation)
        {
            Log.Info("WKWebView navigation finished: {0}", webView.Url);
        }

        public override void DidFailNavigation(WKWebView webView, WKNavigation navigation, NSError error)
        {
            Log.Error("WKWebView navigation failed: {0} for {1}", error.LocalizedDescription, webView.Url);
        }

        public override void DidFailProvisionalNavigation(WKWebView webView, WKNavigation navigation, NSError error)
        {
            Log.Error("WKWebView provisional navigation failed: {0} for {1}", error.LocalizedDescription, webView.Url);
        }
    }
}
