#include "pengu.h"
#include "v8_wrapper.h"
#include <thread>
#include <functional>

static V8Value *v8_dir_exists(V8Value *const args[], int argc)
{
    CefScopedStr dir = args[0]->asString();
    auto subpath = dir.to_path();

    auto task = new V8PromiseTask();
    return task->execute([task, subpath]
        {
            auto path = config::plugins_dir() / subpath;
            bool ret = file::is_dir(path);

            task->resolve([ret] { return V8Value::boolean(ret); });
        }
    );
}

static V8Value *v8_dir_files(V8Value *const args[], int argc)
{
    CefScopedStr dir = args[0]->asString();
    auto subpath = dir.to_path();

    auto task = new V8PromiseTask();
    return task->execute([task, subpath]
        {
            auto path = config::plugins_dir() / subpath;
            auto files = file::read_dir(path);

            for (auto it = files.begin(); it != files.end(); )
            {
                if (it->c_str()[0] == '.' || !file::is_file(path / *it))
                {
                    it = files.erase(it);
                    continue;
                }
                ++it;
            }

            task->resolve([files]
                {
                    int count = (int)files.size();
                    auto array = V8Array::create(count);

                    for (int i = 0; i < count; ++i)
                    {
                        auto str = CefStr::from_path(files[i]);
                        array->set(i, V8Value::string(&str));
                    }

                    return (V8Value *)array;
                });
        }
    );
}

static V8Value *v8_dir_reveal(V8Value *const args[], int argc)
{
    CefScopedStr dir = args[0]->asString();
    bool create = argc > 1 && args[1]->asBool();

    auto subpath = dir.to_path();

    std::thread([subpath, create]
        {
            auto path = config::plugins_dir() / subpath;
            if (!file::is_dir(path))
            {
                if (create)
                    file::make_dir(path);
                else
                    return;
            }

            shell::open_folder(path);
        }
    ).detach();

    return nullptr;
}

V8HandlerFunctionEntry v8_FileSystemEntries[]
{
    { "DirExists", v8_dir_exists },
    { "DirFiles", v8_dir_files },
    { "DirReveal", v8_dir_reveal },
    { nullptr }
};