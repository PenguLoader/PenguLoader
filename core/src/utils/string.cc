#include "../internal.h"

#include <locale>
#include <codecvt>
#include <algorithm>

wstring utils::toWide(const string &str)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> conv;
    return conv.from_bytes(str);
}

string utils::toNarrow(const wstring &wstr)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> conv;
    return conv.to_bytes(wstr);
}

bool utils::strEqual(const wstring &a, const wstring &b, bool sensitive)
{
    if (a.length() != b.length())
        return false;

    if (sensitive)
        return wcsncmp(a.c_str(), b.c_str(), a.length()) == 0;

    wstring a_lower, b_lower;
    a_lower.resize(a.size());
    b_lower.resize(b.size());

    std::transform(a.begin(), a.end(), a_lower.begin(), ::towlower);
    std::transform(b.begin(), b.end(), b_lower.begin(), ::towlower);

    return wcsncmp(a_lower.c_str(), b_lower.c_str(), a.length()) == 0;
}

bool utils::strContain(const wstring &str, const wstring &sub, bool sensitive)
{
    if (sensitive)
        return str.find(sub) != string::npos;

    wstring str_lower, sub_lower;
    str_lower.resize(str.size());
    sub_lower.resize(sub.size());

    std::transform(str.begin(), str.end(), str_lower.begin(), ::towlower);
    std::transform(sub.begin(), sub.end(), sub_lower.begin(), ::towlower);

    return str_lower.find(sub_lower) != string::npos;
}

bool utils::strStartWith(const wstring &str, const wstring &sub)
{
    if (str.length() < sub.length())
        return false;

    return wcsncmp(str.c_str(), sub.c_str(), sub.length()) == 0;
}

bool utils::strEndWith(const wstring &str, const wstring &sub)
{
    if (str.length() < sub.length())
        return false;

    return wcsncmp(str.c_str() + str.length() - sub.length(), sub.c_str(), sub.length()) == 0;
}

wstring utils::encodeBase64(const wstring &str)
{
    constexpr char CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::wstring out{};
    int val = 0, valb = -6;

    for (wchar_t c : str)
    {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0)
        {
            out.push_back(CHARS[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }

    if (valb > -6)
        out.push_back(CHARS[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4)
        out.push_back('=');

    return out;
}