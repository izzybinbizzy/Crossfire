#pragma once
#define _INC_VADEFS
#define _CRT_PACKING 8
#pragma pack(push, _CRT_PACKING)
#ifdef __cplusplus
extern "C" {
#endif
#ifndef _UINTPTR_T_DEFINED
#define _UINTPTR_T_DEFINED
typedef unsigned __int64 uintptr_t;
#endif
#ifndef _VA_LIST_DEFINED
#define _VA_LIST_DEFINED
typedef char* va_list;
#endif
#ifdef __cplusplus
}
#endif
#define _ADDRESSOF(v) (&(v))
#define _SLOTSIZEOF(t) sizeof(__int64)
#define _APALIGN(t,ap) (__alignof(t))
#define __crt_va_start(ap, x) __builtin_va_start(ap, x)
#define __crt_va_arg(ap, t) __builtin_va_arg(ap, t)
#define __crt_va_end(ap) __builtin_va_end(ap)
#define __crt_va_start_a(ap, x) __builtin_va_start(ap, x)
#pragma pack(pop)
