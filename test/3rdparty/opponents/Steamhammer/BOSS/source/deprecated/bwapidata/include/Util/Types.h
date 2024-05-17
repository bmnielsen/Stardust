#pragma once

typedef unsigned char      u8;
typedef signed char        s8;

typedef unsigned short     u16;
typedef signed short       s16;

typedef unsigned int       u32;
typedef signed int         s32;

typedef unsigned long long u64;
typedef signed long long s64;

typedef u8              _UNKNOWN;

typedef unsigned char   BYTE;
typedef unsigned short  WORD;
typedef unsigned long   DWORD;
typedef void*           PVOID;
typedef int             BOOL;
typedef void*           HANDLE;

#ifdef NULL
#undef NULL
#endif
#define NULL 0

#define ever (;;)
