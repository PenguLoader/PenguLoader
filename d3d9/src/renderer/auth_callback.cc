#include "../internal.h"
#include <map>

extern HWND RCLIENT_WINDOW;
extern int server_port_;

using std::pair;
using std::make_pair;
using CallbackMap = std::map<pair<wstring, int>, pair<cef_v8context_t *, cef_v8value_t *>>;

static int callback_count_ = 0;
static CallbackMap callback_map_;
static cef_server_t *server_ = nullptr;

void TriggerAuthCallback(const wstring &url, int browser_id, const wstring &response)
{
    CallbackMap::const_iterator it = callback_map_.find(
        make_pair(url, browser_id));

    if (it != callback_map_.end())
    {
        auto context = it->second.first;
        auto callback = it->second.second;
        callback_map_.erase(it);

        context->enter(context);

        cef_v8value_t *args = CefV8Value_CreateString(&CefStr(response));
        callback->execute_function(callback, nullptr, 1, &args);

        context->exit(context);
    }
}

bool HandleAuthCallback(const wstring &fn, const vector<cef_v8value_t *> &args, cef_v8value_t * &retval)
{
    if (fn == L"CreateAuthCallbackURL")
    {
        wstring url = L"http://127.0.0.1:";
        url.append(std::to_wstring(server_port_));
        url.append(L"/callback/");
        url.append(std::to_wstring(callback_count_++));
        url.append(L"/");

        retval = CefV8Value_CreateString(&CefStr(url));
        return true;
    }
    else if (fn == L"AddAuthCallback")
    {
        if (args.size() >= 2
            && args[0]->is_string(args[0])
            && args[1]->is_function(args[1]))
        {
            CefScopedStr url{ args[0]->get_string_value(args[0]) };

            auto context = CefV8Context_GetCurrentContext();
            auto btowser = context->get_browser(context);
            int browser_id = btowser->get_identifier(btowser);

            callback_map_.insert(make_pair(
                make_pair(wstring(url.str, url.length), browser_id),
                make_pair(context, args[1])
            ));
        }

        return true;
    }
    else if (fn == L"RemoveAuthCallback")
    {
        if (args.size() >= 1 && args[0]->is_string(args[0]))
        {
            CefScopedStr url{ args[0]->get_string_value(args[0]) };

            auto context = CefV8Context_GetCurrentContext();
            auto btowser = context->get_browser(context);
            int browser_id = btowser->get_identifier(btowser);

            CallbackMap::const_iterator it = callback_map_.find(
                make_pair(wstring(url.str, url.length), browser_id));

            if (it != callback_map_.end())
                callback_map_.erase(it);
        }

        return true;
    }

    return false;
}

void ClearAuthCallbacks(cef_v8context_t *context)
{
    if (!callback_map_.empty())
    {
        CallbackMap::iterator it = callback_map_.begin();
        for (; it != callback_map_.end();)
        {
            auto stored = it->second.first;

            if (stored->is_same(stored, context))
                callback_map_.erase(it++);
            else
                ++it;
        }
    }
}