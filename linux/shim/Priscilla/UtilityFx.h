// Subset of Priscilla/UtilityFx used by AtaSmart.cpp.
#pragma once
#include "../windows.h"

void DebugPrint(CString cstr);

#ifndef NODISCARD
#define NODISCARD [[nodiscard]]
#endif

inline ULONG64 B8toB64(BYTE b0, BYTE b1 = 0, BYTE b2 = 0, BYTE b3 = 0, BYTE b4 = 0, BYTE b5 = 0, BYTE b6 = 0, BYTE b7 = 0)
{
	return ((ULONG64)b7 << 56) | ((ULONG64)b6 << 48) | ((ULONG64)b5 << 40) | ((ULONG64)b4 << 32)
	     | ((ULONG64)b3 << 24) | ((ULONG64)b2 << 16) | ((ULONG64)b1 << 8) | (ULONG64)b0;
}
inline DWORD B8toB32(BYTE b0, BYTE b1 = 0, BYTE b2 = 0, BYTE b3 = 0)
{
	return ((DWORD)b3 << 24) | ((DWORD)b2 << 16) | ((DWORD)b1 << 8) | (DWORD)b0;
}
NODISCARD inline ULONG64 B8toB64le_ptr(const BYTE* v) noexcept { ULONG64 u; memcpy(&u, v, sizeof(u)); return u; }
NODISCARD inline DWORD B8toB32le_ptr(const BYTE* v) noexcept { return (DWORD)v[0] | ((DWORD)v[1] << 8) | ((DWORD)v[2] << 16) | ((DWORD)v[3] << 24); }
NODISCARD inline INT B8toINTle_ptr(const BYTE* v) noexcept { return (INT)B8toB32le_ptr(v); }
NODISCARD inline USHORT B8toB16le_ptr(const BYTE* v) noexcept { return (USHORT)(v[0] | (v[1] << 8)); }
NODISCARD inline SHORT B8toSHORTle_ptr(const BYTE* v) noexcept { return (SHORT)B8toB16le_ptr(v); }
NODISCARD inline ULONG64 B8toB64le(const BYTE (&v)[6]) noexcept
{
	return (ULONG64)v[0] | ((ULONG64)v[1] << 8) | ((ULONG64)v[2] << 16) | ((ULONG64)v[3] << 24) | ((ULONG64)v[4] << 32) | ((ULONG64)v[5] << 40);
}
NODISCARD inline DWORD B8toB32le(const BYTE (&v)[6]) noexcept { return B8toB32le_ptr(v); }
NODISCARD inline INT B8toINTle(const BYTE (&v)[6]) noexcept { return (INT)B8toB32le_ptr(v); }
NODISCARD inline USHORT B8toB16le(const BYTE (&v)[6]) noexcept { return B8toB16le_ptr(v); }

ULONGLONG GetTickCountFx();
