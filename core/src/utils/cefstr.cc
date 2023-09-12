#include "commons.h"

// utf8 string helpers

CefStrUtf8::CefStrUtf8() : cef_string_utf8_t{ "", 0, nullptr }
{
}

CefStrUtf8::CefStrUtf8(const cef_string_t *s) : cef_string_utf8_t{}
{
    cef_string_to_utf8(s->str, s->length, this);
}

CefStrUtf8::~CefStrUtf8()
{
    if (dtor != nullptr)
    {
        dtor(str);
    }
}

str CefStrUtf8::cstr() const
{
    return ::str(str, length);
}

// utf16 string helpers

CefStrBase::CefStrBase() : cef_string_t{ L"", 0, nullptr }
{
}

bool CefStrBase::empty() const
{
    return length == 0;
}

bool CefStrBase::equal(const wchar_t *s) const
{
    return wcscmp(str, s) == 0;
}

bool CefStrBase::equal(const wstr &s) const
{
    return wcsncmp(str, s.c_str(), length) == 0;
}

bool CefStrBase::equali(const wchar_t *s) const
{
    return _wcsicmp(str, s) == 0;
}

bool CefStrBase::equali(const wstr &s) const
{
    return _wcsnicmp(str, s.c_str(), length) == 0;
}

bool CefStrBase::search(const wstr &regex, bool icase) const
{
    if (empty()) return false;

    auto flags = icase ? std::regex::icase : std::regex::flag_type(0);
    std::wregex pattern(regex, flags);

    wstr input(str, length);
    return std::regex_search(input, pattern);
}

wstr CefStrBase::cstr() const
{
    return wstr(str, length);
}

CefStr::CefStr() : CefStrBase()
{
}

CefStr::CefStr(const char *s, size_t l) : CefStrBase()
{
    cef_string_from_utf8(s, l, this);
}

CefStr::CefStr(const wchar_t *s, size_t l) : CefStrBase()
{
    cef_string_from_wide(s, l, this);
}

CefStr::CefStr(const ::str &s) : CefStr(s.c_str(), s.length())
{
}

CefStr::CefStr(const wstr &s) : CefStr(s.c_str(), s.length())
{
}

CefStr::~CefStr()
{
    if (dtor != nullptr)
    {
        dtor(str);
    }
}

cef_string_t CefStr::forward()
{
    auto dtor_ = this->dtor;
    this->dtor = nullptr;

    return cef_string_t{ str, length, dtor_ };
}

CefStrBase CefStr::borrow(const cef_string_t *s)
{
    CefStrBase base{};

    if (s != nullptr)
    {
        base.str = s->str;
        base.length = s->length;
    }
    else
    {
        base.str = L"";
        base.length = 0;
    }

    return base;
}

// userfree scoped string

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
    {
        cef_string_userfree_free(str_);
    }
}