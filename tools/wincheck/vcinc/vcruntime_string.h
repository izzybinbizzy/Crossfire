#pragma once
#include <vcruntime.h>
#ifdef __cplusplus
extern "C" {
#endif
_Check_return_ _VCRTIMP _CONST_RETURN void* __cdecl memchr(const void* _Buf, int _Val, size_t _MaxCount);
_Check_return_ int __cdecl memcmp(const void* _Buf1, const void* _Buf2, size_t _Size);
void* __cdecl memcpy(void* _Dst, const void* _Src, size_t _Size);
_VCRTIMP void* __cdecl memmove(void* _Dst, const void* _Src, size_t _Size);
void* __cdecl memset(void* _Dst, int _Val, size_t _Size);
_Check_return_ _VCRTIMP _CONST_RETURN char* __cdecl strchr(const char* _Str, int _Val);
_Check_return_ _VCRTIMP _CONST_RETURN char* __cdecl strrchr(const char* _Str, int _Ch);
_Check_return_ _VCRTIMP _CONST_RETURN char* __cdecl strstr(const char* _Str, const char* _SubStr);
_Check_return_ _VCRTIMP _CONST_RETURN wchar_t* __cdecl wcschr(const wchar_t* _Str, wchar_t _Ch);
_Check_return_ _VCRTIMP _CONST_RETURN wchar_t* __cdecl wcsrchr(const wchar_t* _Str, wchar_t _Ch);
_Check_return_ _VCRTIMP _CONST_RETURN wchar_t* __cdecl wcsstr(const wchar_t* _Str, const wchar_t* _SubStr);
#ifdef __cplusplus
}
#endif
