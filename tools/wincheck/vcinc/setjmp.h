#pragma once
#include <vcruntime.h>
typedef struct __declspec(align(16)) _SETJMP_FLOAT128 { unsigned __int64 Part[2]; } SETJMP_FLOAT128;
typedef SETJMP_FLOAT128 _JBTYPE;
#define _JBLEN 16
typedef _JBTYPE jmp_buf[_JBLEN];
#ifdef __cplusplus
extern "C" {
#endif
int __cdecl _setjmp(jmp_buf _Buf);
#define setjmp(b) _setjmp(b)
__declspec(noreturn) void __cdecl longjmp(jmp_buf _Buf, int _Value) noexcept(false);
#ifdef __cplusplus
}
#endif
