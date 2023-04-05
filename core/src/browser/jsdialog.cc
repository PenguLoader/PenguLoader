#include "../internal.h"
#include "include/capi/cef_jsdialog_handler_capi.h"

extern HWND rclient_window_;

class CustomJSDialogHandler : public CefRefCount<cef_jsdialog_handler_t>
{
public:
    CustomJSDialogHandler() : CefRefCount(this)
    {
        cef_jsdialog_handler_t::on_jsdialog = on_jsdialog;
    }

private:
    struct DialogData
    {
        wstring message;
        cef_jsdialog_type_t type;
        cef_jsdialog_callback_t *callback;
    };

    static int CEF_CALLBACK on_jsdialog(struct _cef_jsdialog_handler_t* self,
        struct _cef_browser_t* browser,
        const cef_string_t* origin_url,
        cef_jsdialog_type_t dialog_type,
        const cef_string_t* message_text,
        const cef_string_t* default_prompt_text,
        struct _cef_jsdialog_callback_t* callback,
        int* suppress_message)
    {
        if (dialog_type == JSDIALOGTYPE_ALERT || dialog_type == JSDIALOGTYPE_CONFIRM)
        {
            auto data = new DialogData();
            data->type = dialog_type;
            data->callback = callback;
            data->message = message_text ? wstring(message_text->str, message_text->length) : L"";

            CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)dialog_thread, data, 0, NULL);
            return 1;
        }

        return 0;
    }

    static void CALLBACK dialog_thread(DialogData *data)
    {
        if (data->type == JSDIALOGTYPE_ALERT)
        {
            MessageBoxW(rclient_window_, data->message.c_str(),
                L"League of Legends Client", MB_ICONINFORMATION | MB_OK);
            data->callback->cont(data->callback, 0, nullptr);
        }
        else
        {
            int ret = MessageBoxW(rclient_window_, data->message.c_str(),
                L"League of Legends Client", MB_ICONQUESTION | MB_YESNO);
            data->callback->cont(data->callback, ret == IDYES, nullptr);
        }

        delete data;
    }
};

cef_jsdialog_handler_t *CreateCustomJSDialogHandler()
{
    return new CustomJSDialogHandler();
}