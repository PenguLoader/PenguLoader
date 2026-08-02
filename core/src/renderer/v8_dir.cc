#include "pengu.h"
#include "v8_wrapper.h"
#include "../browser/assets_path.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

// =============================================================================
// `window.__pdir.{exists,files,reveal}()` — back-end of the `?dir` Directory
// module. The shim in `assets_shims.h SCRIPT_IMPORT_DIR` calls in with NO
// arguments; the target folder is derived from the calling script's own URL,
// read off the V8 stack, which JS cannot forge.
//
// That is the whole security model. A path parameter would make these ambient
// enumerate/create/reveal capabilities for every script in the renderer,
// including remote code a plugin imported — exactly what the Directory module
// exists to prevent. Same mechanism as `caller_json_path` in v8_json_write.cc,
// discriminating on the `?dir` query instead of a `.json` extension.
// =============================================================================

static constexpr size_t URL_PREFIX_LEN = 15;  // "https://plugins"

/// Resolve the plugins-relative path of the `?dir` module that called us, or
/// return false if the caller isn't one.
///
/// Frame 0 is the Directory method defined inside SCRIPT_IMPORT_DIR, so its
/// script name is the module URL including the `?dir` query. A script calling
/// `window.__pdir.*` directly lands its own URL here instead, which carries no
/// `?dir` query and is rejected.
static bool caller_dir_path(std::u16string &out)
{
    auto trace = cef_v8stack_trace_get_current(1);
    if (trace == nullptr)
        return false;

    bool ok = false;

    if (auto frame = trace->get_frame(trace, 0))
    {
        // Reject eval'd frames: their script name is inherited from the
        // surrounding script, so an `eval` reachable inside the module's
        // scope would otherwise borrow its capability.
        if (frame->is_valid(frame) && !frame->is_eval(frame))
        {
            CefScopedStr name = frame->get_script_name(frame);

            if (name.length >= URL_PREFIX_LEN &&
                std::memcmp(name.str, u"https://plugins",
                            URL_PREFIX_LEN * sizeof(char16)) == 0)
            {
                std::u16string rel((char16_t *)name.str + URL_PREFIX_LEN,
                                   name.length - URL_PREFIX_LEN);

                // The `?dir` query is the discriminator — it is what stops a
                // plugin's own `index.js` from calling in and pointing these
                // at its folder.
                if (auto pos = rel.find(u'?'); pos != std::u16string::npos)
                {
                    if (rel.compare(pos + 1, std::u16string::npos, u"dir") == 0)
                    {
                        rel = rel.substr(0, pos);
                        assets::decode_uri(rel);

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

/// Caller's directory, re-validated against the plugins root. The import rules
/// constrain module resolution only and are never assumed here.
static bool caller_dir(path &target)
{
    std::u16string rel;
    if (!caller_dir_path(rel))
        return false;

    target = path{ config::plugins_dir().u16string() + rel };
    return assets::is_inside(config::plugins_dir(), target);
}

static constexpr const char *ERR_CALLER =
    "Directory: caller is not a https://plugins/ ?dir module";

static V8Value *v8_dir_exists(V8Value *const args[], int argc)
{
    auto *task = new V8PromiseTask();
    auto *promise = task->promise();

    path target;
    if (!caller_dir(target))
    {
        task->reject(ERR_CALLER);
        return promise;
    }

    task->execute([task, target] {
        bool found = file::is_dir(target);
        task->resolve([found]() -> V8Value * { return V8Value::boolean(found); });
    });

    return promise;
}

static V8Value *v8_dir_files(V8Value *const args[], int argc)
{
    auto *task = new V8PromiseTask();
    auto *promise = task->promise();

    path target;
    if (!caller_dir(target))
    {
        task->reject(ERR_CALLER);
        return promise;
    }

    task->execute([task, target] {
        // A directory that isn't there yet is the normal state before the user
        // first opens it — that's an empty listing, not a failure. A directory
        // that exists but can't be enumerated is a real error and rejects, so
        // a permissions problem doesn't read as an empty folder.
        if (!file::is_dir(target))
        {
            task->resolve([]() -> V8Value * {
                return reinterpret_cast<V8Value *>(V8Array::create(0));
            });
            return;
        }

        std::error_code ec;
        std::vector<std::u16string> names;

        for (const auto &name : file::read_dir(target))
        {
            if (file::is_file(target / name))
                names.push_back(name.u16string());
        }

        if (!file::is_dir(target))
        {
            task->reject("Directory.files: cannot read directory");
            return;
        }

        // Stable order so plugin UIs don't reshuffle between calls.
        std::sort(names.begin(), names.end());

        // V8 allocation has to happen on the renderer thread inside the
        // captured context, so build the array in the resolver.
        task->resolve([names = std::move(names)]() -> V8Value * {
            auto arr = V8Array::create(static_cast<int>(names.size()));
            for (size_t i = 0; i < names.size(); ++i)
            {
                auto str = CefStr(names[i]);
                arr->set(static_cast<int>(i), V8Value::string(&str));
            }
            return reinterpret_cast<V8Value *>(arr);
        });
    });

    return promise;
}

static V8Value *v8_dir_reveal(V8Value *const args[], int argc)
{
    auto *task = new V8PromiseTask();
    auto *promise = task->promise();

    path target;
    if (!caller_dir(target))
    {
        task->reject(ERR_CALLER);
        return promise;
    }

    task->execute([task, target] {
        // Creation is implicit: reveal() is the "user clicked browse" action,
        // and an empty folder is the right thing to show. create_directories
        // fails if a file occupies the path, which is what we want — handing a
        // file to the shell would try to run it.
        std::error_code ec;
        std::filesystem::create_directories(target, ec);

        if (!file::is_dir(target))
        {
            task->reject("Directory.reveal: cannot create directory");
            return;
        }

        // Open on the renderer thread, matching where openPluginsFolder calls
        // it from — the pool workers have no COM apartment for ShellExecuteW.
        task->resolve([target]() -> V8Value * {
            shell::open_folder(target);
            return nullptr;
        });
    });

    return promise;
}

V8HandlerFunctionEntry v8_DirEntries[]
{
    { "DirExists", v8_dir_exists },
    { "DirFiles",  v8_dir_files  },
    { "DirReveal", v8_dir_reveal },
    { nullptr }
};
