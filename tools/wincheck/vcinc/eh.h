#pragma once
#include <corecrt_terminate.h>
#ifdef __cplusplus
extern "C++" {
typedef void (__cdecl* unexpected_handler)();
struct _EXCEPTION_POINTERS;
typedef void (__cdecl* _se_translator_function)(unsigned int, struct _EXCEPTION_POINTERS*);
_se_translator_function __cdecl _set_se_translator(_se_translator_function _NewSETranslator);
class type_info;
int __cdecl _is_exception_typeof(type_info const& _Type, struct _EXCEPTION_POINTERS* _ExceptionPtr);
bool __cdecl __uncaught_exception();
int __cdecl __uncaught_exceptions();
}
#endif
