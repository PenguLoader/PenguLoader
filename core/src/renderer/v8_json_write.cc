#include "pengu.h"
#include "v8_wrapper.h"
#include "../browser/assets_path.h"

#include <cstdio>
#include <cstring>
#include <string>

// =============================================================================
// `__pengu_write_json(content)` — back-end of the writable-JSON module's
// `data.$write()`. The shim in `assets_shims.h SCRIPT_IMPORT_JSON` calls in
// with the current `JSON.stringify(data)`, and nothing else.
//
// The write target is NOT a parameter. It is derived from the calling
// script's own URL, read off the V8 stack, which JS cannot forge. A JSON
// module can therefore rewrite itself and nothing else.
//
// This matters because `window.__pwj` is a plain global reachable by every
// script in the renderer — including remote code a plugin imported. Taking a
// URL here would hand all of them a write-anywhere-inside-plugins primitive,
// and since the write replaces a file's whole content and renderer.cc loads
// every `<plugin>/index.js` at launch, that is persistent code execution
// rather than mere config tampering.
// =============================================================================

static constexpr size_t URL_PREFIX_LEN = 15;  // "https://plugins"

/// Resolve the plugins-relative path of the script that called us, or return
/// false if that script isn't a JSON module served off `https://plugins/`.
///
/// Frame 0 is the `$write` arrow function defined inside SCRIPT_IMPORT_JSON,
/// so its script name is the .json module's own URL. A script calling
/// `window.__pwj` directly lands its own URL here instead: a remote script
/// fails the prefix test, and a plugin's `index.js` fails the extension test.
static bool caller_json_path(std::u16string &out)
{
    auto trace = cef_v8stack_trace_get_current(1);
    if (trace == nullptr)
        return false;

    bool ok = false;

    if (auto frame = trace->get_frame(trace, 0))
    {
        // Reject eval'd frames: their script name is inherited from the
        // surrounding script, so an `eval` reachable inside a JSON module's
        // scope would otherwise borrow its write capability.
        if (frame->is_valid(frame) && !frame->is_eval(frame))
        {
            CefScopedStr name = frame->get_script_name(frame);

            if (name.length >= URL_PREFIX_LEN &&
                std::memcmp(name.str, u"https://plugins",
                            URL_PREFIX_LEN * sizeof(char16)) == 0)
            {
                std::u16string rel((char16_t *)name.str + URL_PREFIX_LEN,
                                   name.length - URL_PREFIX_LEN);

                // Strip query (matches the resource handler's behavior).
                if (auto pos = rel.find(u'?'); pos != std::u16string::npos)
                    rel = rel.substr(0, pos);

                assets::decode_uri(rel);

                // Must be a .json module. This is what stops a plugin's own
                // `index.js` from calling in directly and writing itself.
                if (rel.length() > 5)
                {
                    auto ext = rel.substr(rel.length() - 5);
                    for (auto &ch : ext)
                        if (ch >= u'A' && ch <= u'Z') ch = ch - u'A' + u'a';

                    if (ext == u".json")
                    {
                        out = std::move(rel);
                        ok = true;
                    }
                }
            }
        }

        frame->base.release(&frame->base);
    }

    trace->base.release(&trace->base);
    return ok;
}

static V8Value *v8_write_json(V8Value *const args[], int argc)
{
    auto *task = new V8PromiseTask();
    auto *promise = task->promise();

    // Validate arity + types. Reject syncronously (the reject still hops to
    // TID_RENDERER, but we don't burn a worker slot).
    if (argc < 1 || !args[0]->isString())
    {
        task->reject("WriteJson: expected (content: string)");
        return promise;
    }

    CefScopedStr content = args[0]->asString();

    std::u16string rel;
    if (!caller_json_path(rel))
    {
        task->reject("WriteJson: caller is not a https://plugins/ JSON module");
        return promise;
    }

    auto full = config::plugins_dir().u16string() + rel;
    path target{ full };

    // Path-traversal sandbox. Same helper the resource handler uses.
    if (!assets::is_inside(config::plugins_dir(), target))
    {
        task->reject("WriteJson: path escapes plugins directory");
        return promise;
    }

    // UTF-16 → UTF-8 for the on-disk bytes. Done here (renderer thread) so
    // the worker only does file I/O.
    cef_string_utf8_t utf8{};
    cef_string_to_utf8(content.str, content.length, &utf8);
    std::string body(utf8.str, utf8.length);
    cef_string_utf8_clear(&utf8);

    task->execute([task, target, body = std::move(body)] {
        if (file::atomic_write(target, body.data(), body.size()))
            task->resolve();
        else
            task->reject("WriteJson: write failed");
    });

    return promise;
}

V8HandlerFunctionEntry v8_JsonWriteEntries[]
{
    { "WriteJson", v8_write_json },
    { nullptr }
};
