#include "pengu.h"

// utf16 string helpers

CefStrBase::CefStrBase() : cef_string_t{ (char16 *)u"", 0, nullptr }
{
}

bool CefStrBase::empty() const
{
    return length == 0;
}

bool CefStrBase::equal(const char *that) const
{
    size_t i;
    for (i = 0; that[i] != '\0'; ++i) {
        if (i >= length)
            return false;
        if (str[i] != that[i])
            return false;
    }

    return length == i;
}

bool CefStrBase::contain(const char *sub) const
{
    if (empty()) return false;
    size_t sub_len = strlen(sub);

    for (size_t i = 0; i <= length - sub_len; ++i) {
        bool match = true;
        for (size_t j = 0; j < sub_len; ++j) {
            if (str[i + j] != sub[j]) {
                match = false;
                break;
            }
        }
        if (match)
            return true;
    }

    return false;
}

bool CefStrBase::startw(const char *sub) const
{
    if (empty()) return false;

    size_t i = 0;
    while (sub[i] != '\0') {
        if (str[i] == '\0')
            return false;
        if (str[i] != sub[i])
            return false;
        i++;
    }
    return true;
}

bool CefStrBase::endw(const char *sub) const
{
    if (empty()) return false;

    size_t sub_len = strlen(sub);
    if (length < sub_len)
        return false;

    size_t si = length - sub_len;
    for (size_t i = 0; i < sub_len; ++i) {
        if (str[si + i] != sub[i])
            return false;
    }

    return true;
}

void CefStrBase::copy(std::u16string &to) const
{
    to.assign((char16_t *)str, length);
}

std::string CefStrBase::to_utf8() const
{
    cef_string_utf8_t out{};
    cef_string_to_utf8(str, length, &out);

    std::string ret(out.str, out.length);
    cef_string_utf8_clear(&out);
    return ret;
}

std::u16string CefStrBase::to_utf16() const
{
    return std::u16string((const char16_t *)str, length);
}

std::filesystem::path CefStrBase::to_path() const
{
#if OS_WIN
    return std::filesystem::path(std::wstring(str, length));
#else
    return std::filesystem::path(to_utf8());
#endif
}

CefStr::CefStr() : CefStrBase()
{
}

CefStr::CefStr(const char *s, size_t len) : CefStrBase()
{
    cef_string_from_utf8(s, len, this);
}

CefStr::CefStr(const char16_t *s, size_t len) : CefStrBase()
{
    cef_string_from_utf16((char16 *)s, len, this);
}

CefStr::CefStr(const std::string &s) : CefStr(s.c_str(), s.length())
{
}

CefStr::CefStr(const std::u16string &s) : CefStr(s.c_str(), s.length())
{
}

CefStr CefStr::from_path(const path &path)
{
#if OS_WIN
    return CefStr(path.u16string());
#else
    return CefStr(path.string());
#endif
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

const CefStrBase &CefStr::borrow(const cef_string_t *s)
{
    if (s != nullptr)
    {
        return *reinterpret_cast<const CefStrBase *>(s);
    }
    else
    {
        static CefStrBase empty{};
        return empty;
    }
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
        cef_string_t::str = (char16 *)u"";
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