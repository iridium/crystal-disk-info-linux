// Minimal Win32/MFC compatibility layer so CrystalDiskInfo's platform-neutral
// logic (AtaSmart.cpp vendor detection, health status, NVMe interpreter)
// compiles unchanged on Linux. Only what that code touches is provided.
#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <cwctype>
#include <cstdarg>
#include <string>
#include <vector>
#include <algorithm>
#include <time.h>

// ---------------------------------------------------------------------------
// Basic types
// ---------------------------------------------------------------------------
typedef int32_t             BOOL;
typedef uint8_t             BYTE, UCHAR, BOOLEAN;
typedef uint16_t            WORD, USHORT;
typedef int16_t             SHORT;
typedef uint32_t            DWORD, ULONG, UINT;
typedef int32_t             INT, LONG;
typedef int64_t             LONGLONG, LONG64, INT64;
typedef uint64_t            ULONGLONG, ULONG64, DWORD64, UINT64;
typedef uintptr_t           ULONG_PTR, DWORD_PTR, UINT_PTR, SIZE_T;
typedef intptr_t            LONG_PTR, INT_PTR;
typedef char                CHAR;
typedef wchar_t             WCHAR, TCHAR;
typedef void                VOID;
typedef void*               PVOID;
typedef void*               LPVOID;
typedef void*               HANDLE;
typedef void*               HMODULE;
typedef BYTE*               PBYTE;
typedef BYTE*               LPBYTE;
typedef WORD*               PWORD;
typedef DWORD*              PDWORD;
typedef DWORD*              LPDWORD;
typedef BOOL*               PBOOL;
typedef CHAR*               PCHAR;
typedef CHAR*               LPSTR;
typedef const CHAR*         LPCSTR;
typedef WCHAR*              LPWSTR;
typedef const WCHAR*        LPCWSTR;
typedef TCHAR*              LPTSTR;
typedef const TCHAR*        LPCTSTR;
typedef ULONG*              PULONG;
typedef UCHAR*              PUCHAR;
typedef DWORD               COLORREF;
typedef long                HRESULT;

#define CONST const
#define TRUE  1
#define FALSE 0
#define WINAPI
#define __stdcall
#define __cdecl
#define CALLBACK
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)

#ifndef _WIN64
#define _WIN64 1
#endif

#define _T(x)  L##x
#define TEXT(x) L##x

#define LOBYTE(w)  ((BYTE)(((DWORD_PTR)(w)) & 0xff))
#define HIBYTE(w)  ((BYTE)((((DWORD_PTR)(w)) >> 8) & 0xff))
#define LOWORD(l)  ((WORD)(((DWORD_PTR)(l)) & 0xffff))
#define HIWORD(l)  ((WORD)((((DWORD_PTR)(l)) >> 16) & 0xffff))
#define MAKEWORD(a, b) ((WORD)(((BYTE)(((DWORD_PTR)(a)) & 0xff)) | ((WORD)((BYTE)(((DWORD_PTR)(b)) & 0xff))) << 8))
#define MAKELONG(a, b) ((LONG)(((WORD)(((DWORD_PTR)(a)) & 0xffff)) | ((DWORD)((WORD)(((DWORD_PTR)(b)) & 0xffff))) << 16))
#define RGB(r,g,b) ((COLORREF)(((BYTE)(r)|((WORD)((BYTE)(g))<<8))|(((DWORD)(BYTE)(b))<<16)))

#define ZeroMemory(p, n)       memset((p), 0, (n))
#define FillMemory(p, n, v)    memset((p), (v), (n))
#define CopyMemory(d, s, n)    memcpy((d), (s), (n))
#define _countof(a)            (sizeof(a) / sizeof((a)[0]))

inline int memcpy_s(void* dst, size_t dstSize, const void* src, size_t count)
{
	if (count > dstSize) count = dstSize;
	memcpy(dst, src, count);
	return 0;
}

#define _tcscmp   wcscmp
#define _tcsicmp  wcscasecmp
#define _tcslen   wcslen
#define _tcsstr   wcsstr
#define _tcsncmp  wcsncmp
#define _tstoi(s) ((int)wcstol((s), nullptr, 10))
#define _ttoi(s)  ((int)wcstol((s), nullptr, 10))
#define _wtoi(s)  ((int)wcstol((s), nullptr, 10))
#define _tcstol   wcstol
#define _tcstoul  wcstoul
#define _tcscpy_s(d, n, s) wcsncpy((d), (s), (n))
#define wcscpy_s(d, n, s)  wcsncpy((d), (s), (n))
#define strcpy_s(d, n, s)  strncpy((d), (s), (n))
#define _stricmp  strcasecmp
#define _tstof(s) wcstod((s), nullptr)
#define _ttof(s)  wcstod((s), nullptr)
inline int strncpy_s(char* dst, size_t dstSize, const char* src, size_t count)
{
	if (!dstSize) return 0;
	size_t n = std::min(dstSize - 1, count);
	size_t i = 0;
	for (; i < n && src[i]; ++i) dst[i] = src[i];
	dst[i] = 0;
	return 0;
}
#define _wcsicmp  wcscasecmp

inline void Sleep(DWORD ms) { struct timespec ts{ (time_t)(ms / 1000), (long)(ms % 1000) * 1000000L }; nanosleep(&ts, nullptr); }
ULONGLONG GetTickCount64();
inline DWORD GetTickCount() { return (DWORD)GetTickCount64(); }

#include "cstring_shim.h"
