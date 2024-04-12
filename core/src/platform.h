#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef OS_WIN
#define OS_WIN 1
#endif
#elif defined(__APPLE__) || defined(__MACH__)
#ifndef OS_MAC
#define OS_MAC 1
#endif
#else
#error "Your platform is not supported."
#endif

#if !(defined(_M_X64) || defined(_M_AMD64) || defined(__x86_64__) || defined(__amd64__))
#error "Target 64-bit (x86-64/amd64) only."
#endif

#ifdef _MSC_VER
#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS
#endif

#ifndef COUNT_OF
#define COUNT_OF(arr) (sizeof(arr) / sizeof(*arr))
#endif

#if OS_WIN
#define PLATFORM_NAME "win"
#define LIBCEF_MODULE_NAME "libcef.dll"
#elif OS_MAC
#define PLATFORM_NAME "mac"
#define LIBCEF_MODULE_NAME "Chromium Embedded Framework.framework/Chromium Embedded Framework"
#endif

#ifndef NDEBUG
#define _DEBUG 1
#endif

#endif