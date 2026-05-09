using Foundation;
using Pengu.Logging;
using Pengu.Pack;
using WebKit;

namespace Pengu.MacOS.Browser;

/// <summary>
/// WKURLSchemeHandler for <c>app://</c>. Routes <c>app://hub/&lt;path&gt;</c>
/// requests to the packed <see cref="AppDat"/> bundle. Counterpart of
/// <c>Pengu.Windows.Browser.AppSchemeHandler</c>; the bundle format and
/// 404/200 semantics match.
///
/// <para>Only registered when running in "packed" mode (i.e. <c>app.dat</c>
/// is on disk next to the binary). Dev mode bypasses this entirely and
/// navigates straight to the Vite dev server.</para>
/// </summary>
internal sealed class HubAssetSchemeHandler : NSObject, IWKUrlSchemeHandler
{
    private readonly AppDat? _dat;

    /// <summary>
    /// Construct with an optional <see cref="AppDat"/>. When null (no packed
    /// bundle on disk and no <c>--dev</c> URL), every request 404s — that
    /// keeps WKWebView's error path inside the webview rather than escalating
    /// to <c>NSWorkspace</c>'s "what app handles <c>app://</c>?" prompt.
    /// </summary>
    public HubAssetSchemeHandler(AppDat? dat) { _dat = dat; }

    [Export("webView:startURLSchemeTask:")]
    public void StartUrlSchemeTask(WKWebView webView, IWKUrlSchemeTask urlSchemeTask)
    {
        var requestUrl = urlSchemeTask.Request.Url!;
        try
        {
            // app://hub/path/to/file -> path = "path/to/file" (NSUrl.Path
            // strips the leading slash and the host). Empty path -> index.html.
            var path = requestUrl.Path?.TrimStart('/');
            if (string.IsNullOrEmpty(path)) path = "index.html";

            if (_dat is null || !_dat.TryRead(path, out var bytes, out var mime))
            {
                Log.Debug("app:// 404 {0} (dat={1})", requestUrl.AbsoluteString, _dat is null ? "null" : "loaded");
                Respond(urlSchemeTask, requestUrl, statusCode: 404, body: [], mime: "text/plain; charset=utf-8");
                return;
            }

            Respond(urlSchemeTask, requestUrl, statusCode: 200, body: bytes, mime: mime);
        }
        catch (Exception ex)
        {
            Log.Error(ex, "app:// scheme handler threw for {0}", requestUrl.AbsoluteString);
            try
            {
                urlSchemeTask.DidFailWithError(new NSError(
                    new NSString("PenguHubAssetError"), 500,
                    NSDictionary.FromObjectAndKey(new NSString(ex.Message), NSError.LocalizedDescriptionKey)));
            }
            catch
            {
                // urlSchemeTask may already be terminated (web view disposed mid-request)
            }
        }
    }

    [Export("webView:stopURLSchemeTask:")]
    public void StopUrlSchemeTask(WKWebView webView, IWKUrlSchemeTask urlSchemeTask)
    {
        // No long-running work; nothing to cancel.
    }

    private static void Respond(IWKUrlSchemeTask task, NSUrl url, int statusCode, byte[] body, string mime)
    {
        var headers = new NSMutableDictionary
        {
            [(NSString)"Content-Type"]   = (NSString)mime,
            [(NSString)"Content-Length"] = (NSString)body.Length.ToString(),
            // CSP isn't enforced for custom schemes by default; we don't add
            // Access-Control-Allow-Origin because the hub renderer's origin
            // is app://hub/ (same as the requests it makes).
        };
        var response = new NSHttpUrlResponse(url, statusCode, "HTTP/1.1", headers);
        task.DidReceiveResponse(response);
        if (body.Length > 0)
            task.DidReceiveData(NSData.FromArray(body));
        task.DidFinish();
    }
}
