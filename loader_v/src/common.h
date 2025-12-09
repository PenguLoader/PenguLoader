#pragma once

#define WIN32_LEAN_AND_MEAN

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <string>

#undef MAX_PATH
#define MAX_PATH 2048

EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define THIS_MODULE ((HMODULE)&__ImageBase)

#define ShowWraning(fmt, ...) { \
    char msg[256]; \
    sprintf_s(msg, "[%s]: " fmt, __FUNCTION__, ##__VA_ARGS__); \
    MessageBoxA(NULL, msg, "Pengu [loader_v]", MB_ICONWARNING | MB_OK | MB_TOPMOST); \
}

#define DebugLog(fmt, ...) \
	printf("[loader_v][%s]: " fmt "\n", __FUNCTION__, ##__VA_ARGS__)

static bool _wcsi_endw(LPCWSTR str, LPCWSTR suffix)
{
	size_t str_len = wcslen(str);
	size_t suffix_len = wcslen(suffix);
	if (str_len < suffix_len)
		return false;
	return _wcsicmp(str + (str_len - suffix_len), suffix) == 0;
}
