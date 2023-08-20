#include "commons.h"
#include <fstream>
#include <filesystem>

namespace PluginFS {
    template <typename T>
    class StreamGuard
    {
    public:
        StreamGuard(T& stream) : stream_(stream) {}
        ~StreamGuard()
        {
            if (stream_.is_open())
            {
                stream_.close();
            }
        }

    private:
        T& stream_;
    };

    static void removeTailingSlash(wstr& path) {
        if (!path.empty() && path.back() == L'/')
        {
            path.pop_back();
        }
    }

    static str ReadFile(wstr path)
    {
        std::fstream inputStream;
        inputStream.open(path, std::ios::in);

        if (!inputStream.good())
        {
            return {};
        }

        PluginFS::StreamGuard<std::fstream> guard(inputStream);

        str content(std::istreambuf_iterator<char>{inputStream}, {});
        return std::move(content);
    }

    static bool WriteFile(wstr path, str& content, bool enableAppendMode)
    {
        std::fstream outputStream;
        if (enableAppendMode) {
            outputStream.open(path, std::ios::out | std::ios::app);
        }
        else {
            outputStream.open(path, std::ios::out);
        }

        if (!outputStream.good())
        {
            return false;
        }

        PluginFS::StreamGuard<std::fstream> guard(outputStream);

        outputStream << content;
        if (outputStream.fail() || outputStream.bad())
        {
            return false;
        }

        return true;
    }

    static bool MkDir(wstr pluginRoot, wstr relativePath)
    {
        PluginFS::removeTailingSlash(relativePath);
        std::filesystem::path fullPath{ pluginRoot + L"\\" + relativePath };
        if (std::filesystem::exists(fullPath)) {
            return false;
        }
        if (std::filesystem::create_directories(fullPath)) {
            return true;
        }
        return false;
    }

    using FileStat = struct {
        wstr fileName;
        bool isDir;
        int size;
    };

    static PluginFS::FileStat Stat(wstr path)
    {
        PluginFS::removeTailingSlash(path);
        auto entry = std::filesystem::directory_entry(path);

        return PluginFS::FileStat{
            entry.is_regular_file() ? entry.path().filename().wstring() : entry.path().stem(),
            entry.is_directory(),
            static_cast<int>(entry.file_size())
        };
    }

    static vec<wstr> ReadDir(wstr path) {
        PluginFS::removeTailingSlash(path);

        vec<wstr> fileNames;
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            wstr enryPathWstr = entry.path().wstring();
            PluginFS::removeTailingSlash(enryPathWstr);
            std::filesystem::path entryPath{ enryPathWstr };

            fileNames.push_back(entry.is_regular_file() ? entryPath.filename().wstring() : entryPath.stem());
        }
        return std::move(fileNames);
    }

    static int RemoveFile(wstr path, bool recursively) {
        PluginFS::removeTailingSlash(path);

        auto isDir = std::filesystem::is_directory(path);
        if (recursively && isDir) {
            return static_cast<int>(std::filesystem::remove_all(path));
        }
        else {
            return std::filesystem::remove(path);
        }
    }
}

V8Value* native_ReadFile(const vec<V8Value*>& args)
{
    wstr destPath = config::pluginsDir() + L"\\" + args[0]->asString()->str;
    if (std::filesystem::is_regular_file(destPath)) {
        str content = PluginFS::ReadFile(destPath);
        return V8Value::string(&CefStr{ content });
    }
    return V8Value::undefined();
}

V8Value* native_WriteFile(const vec<V8Value*>& args)
{
    wstr destPath = config::pluginsDir() + L"\\" + args[0]->asString()->str;
    str content = CefString(args[1]->asString()).ToString();
    bool enableAppMode = args[2]->asBool();

    if (PluginFS::WriteFile(destPath, std::move(content), enableAppMode)) {
        return V8Value::boolean(true);
    }
    return V8Value::boolean(false);
}

V8Value* native_MkDir(const vec<V8Value*>& args)
{
    wstr pluginsDir = config::pluginsDir();
    wstr pluginName = args[0]->asString()->str;
    wstr relativePath = args[1]->asString()->str;

    if (PluginFS::MkDir(pluginsDir + L"\\" + pluginName, relativePath)) {
        return V8Value::boolean(true);
    }
    return V8Value::boolean(false);
}

V8Value* native_Stat(const vec<V8Value*>& args) {
    wstr destPath = config::pluginsDir() + L"\\" + args[0]->asString()->str;
    if (!std::filesystem::exists(destPath)) {
        return V8Value::undefined();
    }
    PluginFS::FileStat fileStat = PluginFS::Stat(destPath);

    V8Object* v8Obj = V8Object::create();
    v8Obj->set(&L"length"_s, V8Value::number(fileStat.size), V8_PROPERTY_ATTRIBUTE_READONLY);
    v8Obj->set(&L"isDir"_s, V8Value::boolean(fileStat.isDir), V8_PROPERTY_ATTRIBUTE_READONLY);
    v8Obj->set(&L"fileName"_s, V8Value::string(&CefStr{ fileStat.fileName }), V8_PROPERTY_ATTRIBUTE_READONLY);

    return (V8Value*)v8Obj;
}

V8Value* native_ReadDir(const vec<V8Value*>& args) {
    wstr destPath = config::pluginsDir() + L"\\" + args[0]->asString()->str;
    if (!std::filesystem::exists(destPath)) {
        return V8Value::undefined();
    }
    vec<wstr> fileNames = PluginFS::ReadDir(destPath);

    V8Array* v8Array = V8Array::create(fileNames.size());
    for (size_t i = 0; i < fileNames.size(); i++)
    {
        wstr fileName = fileNames[i];
        auto fileNameV8Str = V8Value::string(&CefStr{ fileName });
        v8Array->set(i, fileNameV8Str);
    }
    return (V8Value*)v8Array;
}

V8Value* native_Remove(const vec<V8Value*>& args) {
    wstr destPath = config::pluginsDir() + L"\\" + args[0]->asString()->str;
    bool recursively = args[1]->asBool();

    int ret = PluginFS::RemoveFile(destPath, recursively);
    return V8Value::number(ret);
}