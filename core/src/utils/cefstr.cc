#include "../internal.h"

bool CefStrBase::empty() const
{
    return length == 0;
}

bool CefStrBase::equal(const wchar_t *s) const
{
    return wcscmp(str, s) == 0;
}

bool CefStrBase::equal(const std::wstring &s) const
{
    return wcsncmp(str, s.c_str(), length) == 0;
}

bool CefStrBase::equali(const wchar_t *s) const
{
    return _wcsicmp(str, s) == 0;
}

bool CefStrBase::equali(const std::wstring &s) const
{
    return _wcsnicmp(str, s.c_str(), length) == 0;
}

bool CefStrBase::contain(const wchar_t *s) const
{
    return utils::strContain(str, s);
}

bool CefStrBase::contain(const std::wstring &s) const
{
    return utils::strContain(str, s);
}

bool CefStrBase::operator ==(const wchar_t *s) const
{
    return equal(s);
}

CefStr::CefStr(const char *s, size_t l) : CefStrBase(), owner_(true)
{
    CefString_FromUtf8(s, l, this);
}

CefStr::CefStr(const wchar_t *s, size_t l) : CefStrBase(), owner_(true)
{
    CefString_FromWide(s, l, this);
}

CefStr::CefStr(const std::string &s) : CefStr(s.c_str(), s.length())
{
}

CefStr::CefStr(const std::wstring &s) : CefStr(s.c_str(), s.length())
{
}

CefStr::~CefStr()
{
    if (owner_)
        CefString_Clear(this);
}

CefStr &CefStr::forawrd()
{
    owner_ = false;
    return *this;
}

CefScopedStr::CefScopedStr(cef_string_userfree_t uf) : CefStrBase(), str_(uf)
{
    if (uf != nullptr)
    {
        cef_string_t::str = uf->str;
        cef_string_t::length = uf->length;
    }
    else
    {
        cef_string_t::str = L"";
        cef_string_t::length = 0;
    }
}

CefScopedStr::~CefScopedStr()
{
    if (str_ != nullptr)
        CefString_UserFree_Free(str_);
}