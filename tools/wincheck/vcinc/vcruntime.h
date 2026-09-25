#pragma once
#define _VCRUNTIME_H
#define _VCRT_COMPILER_PREPROCESSOR 1
#define _NODISCARD [[nodiscard]]
#define __CLR_OR_THIS_CALL
#define __CLRCALL_OR_CDECL __cdecl
#define __CLRCALL_PURE_OR_CDECL __cdecl
#define __CLRCALL_PURE_OR_STDCALL __stdcall
#define _VCRT_EXPORT_STD
#define _MSVC_CONSTEXPR
#define _CRT_SATELLITE_1
#define _CRT_SATELLITE_2
#define _CRT_SATELLITE_CODECVT_IDS
#define _CRT_SATELLITE_CODECVT_IDS_NOIMPORT
#ifdef _MSVC_LANG
#define _STL_LANG _MSVC_LANG
#else
#define _STL_LANG __cplusplus
#endif
#ifndef _HAS_CXX17
#if _STL_LANG > 201402L
#define _HAS_CXX17 1
#else
#define _HAS_CXX17 0
#endif
#endif
#ifndef _HAS_CXX20
#if _HAS_CXX17 && _STL_LANG > 201703L
#define _HAS_CXX20 1
#else
#define _HAS_CXX20 0
#endif
#endif
#ifndef _HAS_CXX23
#if _HAS_CXX20 && _STL_LANG > 202002L
#define _HAS_CXX23 1
#else
#define _HAS_CXX23 0
#endif
#endif
#ifndef _HAS_NODISCARD
#define _HAS_NODISCARD 1
#endif
#include <sal.h>
#include <vadefs.h>
#pragma pack(push, _CRT_PACKING)
#define _VCRUNTIME_DISABLED_WARNING_4339_4412
#define _VCRUNTIME_DISABLED_WARNINGS
#define _VCRUNTIME_EXTRA_DISABLED_WARNINGS
#ifndef _VCRTIMP
#define _VCRTIMP
#endif
#ifndef _CRTIMP
#define _VCRT_DEFINED_CRTIMP
#define _CRTIMP
#endif
#define _CRT_BEGIN_C_HEADER __pragma(pack(push, _CRT_PACKING)) extern "C" {
#define _CRT_END_C_HEADER } __pragma(pack(pop))
#ifndef __cplusplus
#undef _CRT_BEGIN_C_HEADER
#undef _CRT_END_C_HEADER
#define _CRT_BEGIN_C_HEADER
#define _CRT_END_C_HEADER
#endif
#define _CRT_BEGIN_C_HEADER_ONLY
#ifdef __cplusplus
#define __vcrt_bool bool
#define _CRT_NOEXCEPT noexcept
#else
#define _CRT_NOEXCEPT
#endif

#ifndef _HAS_EXCEPTIONS
#define _HAS_EXCEPTIONS 1
#endif
#define _CRT_STRINGIZE_(x) #x
#define _CRT_STRINGIZE(x) _CRT_STRINGIZE_(x)
#define _CRT_WIDE_(s) L ## s
#define _CRT_WIDE(s) _CRT_WIDE_(s)
#define _CRT_CONCATENATE_(a, b) a ## b
#define _CRT_CONCATENATE(a, b) _CRT_CONCATENATE_(a, b)
#define _CRT_UNPARENTHESIZE_(...) __VA_ARGS__
#define _CRT_UNPARENTHESIZE(...) _CRT_UNPARENTHESIZE_ __VA_ARGS__
#define _VCRT_ALIGN(x) __declspec(align(x))
#define _CRT_ALIGN(x) __declspec(align(x))
#define __CRTDECL __cdecl
#define _CRT_DEPRECATE_TEXT(_Text) __declspec(deprecated(_Text))
#define _CRT_INSECURE_DEPRECATE(_Replacement)
#define _CRT_INSECURE_DEPRECATE_MEMORY(_Replacement)
#define _CRT_SECURE_CPP_NOTHROW throw()
#define _CRT_GUARDOVERFLOW
#define _CRT_HYBRIDPATCHABLE
#define _CRT_JIT_INTRINSIC
#define _CRTRESTRICT __declspec(restrict)
#define _CRTALLOCATOR __declspec(allocator)
#define _CONST_RETURN const
#define _CRT_CONST_CORRECT_OVERLOADS
#define _VCRT_NOALIAS __declspec(noalias)
#define _VCRT_RESTRICT __declspec(restrict)
#define _VCRT_ALLOCATOR __declspec(allocator)
#define _CRT_UNUSED(x) (void)x
#define _CRT_HAS_CXX17 1
#define _CRT_FUNCTIONS_REQUIRED 1
#ifndef _CRT_BUILD_DESKTOP_APP
#define _CRT_BUILD_DESKTOP_APP 1
#endif
#ifdef __cplusplus
extern "C++" {
template <typename _CountofType, size_t _SizeOfArray> char (*__countof_helper(_CountofType (&_Array)[_SizeOfArray]))[_SizeOfArray];
#define __crt_countof(_Array) (sizeof(*__countof_helper(_Array)) + 0)
}
#else
#define __crt_countof(_Array) (sizeof(_Array) / sizeof(_Array[0]))
#endif
#ifdef __cplusplus
extern "C" {
#endif
#ifndef _SIZE_T_DEFINED
#define _SIZE_T_DEFINED
typedef unsigned __int64 size_t;
typedef __int64 ptrdiff_t;
typedef __int64 intptr_t;
#endif
#ifdef __cplusplus
typedef bool __vcrt_bool_t;
#endif
#ifndef _WCHAR_T_DEFINED
#define _WCHAR_T_DEFINED
#endif
#ifndef NULL
#ifdef __cplusplus
#define NULL 0
#else
#define NULL ((void*)0)
#endif
#endif
void __cdecl __security_init_cookie(void);
void __cdecl __security_check_cookie(uintptr_t _StackCookie);
__declspec(noreturn) void __cdecl __report_gsfailure(uintptr_t _StackCookie);
extern uintptr_t __security_cookie;
#ifdef __cplusplus
}
#endif
#ifndef _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES
#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES 0
#endif
#ifndef _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES_COUNT
#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES_COUNT 0
#endif
#ifndef _CRT_SECURE_CPP_OVERLOAD_SECURE_NAMES
#define _CRT_SECURE_CPP_OVERLOAD_SECURE_NAMES 1
#endif
#pragma pack(pop)
