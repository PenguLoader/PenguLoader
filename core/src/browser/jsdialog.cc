#include "../internal.h"
#include "include/capi/cef_jsdialog_handler_capi.h"

extern HWND rclient_window_;

class CustomJSDialogHandler : public CefRefCount<cef_jsdialog_handler_t>
{
public:
    CustomJSDialogHandler() : CefRefCount(this)
    {
        cef_jsdialog_handler_t::on_jsdialog = on_jsdialog;
        cef_jsdialog_handler_t::on_before_unload_dialog = on_before_unload_dialog;
        cef_jsdialog_handler_t::on_reset_dialog_state = on_reset_dialog_state;
        cef_jsdialog_handler_t::on_dialog_closed = on_dialog_closed;
    }

private:
    static int CEF_CALLBACK on_jsdialog(struct _cef_jsdialog_handler_t* self,
        struct _cef_browser_t* browser,
        const cef_string_t* origin_url,
        cef_jsdialog_type_t dialog_type,
        const cef_string_t* message_text,
        const cef_string_t* default_prompt_text,
        struct _cef_jsdialog_callback_t* callback,
        int* suppress_message)
    {
        LPCWSTR message = L"";
        if (message_text)
            message = message_text->str;

        if (dialog_type == JSDIALOGTYPE_ALERT)
        {
            MessageBoxW(rclient_window_, message, L"League of Legends Client", MB_ICONINFORMATION | MB_OK);
            callback->cont(callback, 0, nullptr);
            return true;
        }
        else if (dialog_type == JSDIALOGTYPE_CONFIRM)
        {
            int ret = MessageBoxW(rclient_window_, message, L"League of Legends Client", MB_ICONQUESTION | MB_YESNO);
            callback->cont(callback, ret == IDYES, nullptr);
            return true;
        }

        return 0;
    }

    static int CEF_CALLBACK on_before_unload_dialog(
        struct _cef_jsdialog_handler_t* self,
        struct _cef_browser_t* browser,
        const cef_string_t* message_text,
        int is_reload,
        struct _cef_jsdialog_callback_t* callback)
    {
        return 0;
    }

    static void CEF_CALLBACK on_reset_dialog_state(
        struct _cef_jsdialog_handler_t* self,
        struct _cef_browser_t* browser)
    {

    }

    static void CEF_CALLBACK on_dialog_closed(struct _cef_jsdialog_handler_t* self,
        struct _cef_browser_t* browser)
    {

    }
};

cef_jsdialog_handler_t *CreateCustomJSDialogHandler()
{
    return new CustomJSDialogHandler();
}