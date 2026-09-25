#pragma once
#include <vcruntime.h>
typedef enum _EXCEPTION_DISPOSITION { ExceptionContinueExecution, ExceptionContinueSearch, ExceptionNestedException, ExceptionCollidedUnwind } EXCEPTION_DISPOSITION;
#ifdef __cplusplus
extern "C" {
#endif
unsigned long __cdecl _exception_code(void);
void* __cdecl _exception_info(void);
int __cdecl _abnormal_termination(void);
#ifdef __cplusplus
}
#endif
#define GetExceptionCode _exception_code
#define exception_code _exception_code
#define GetExceptionInformation (struct _EXCEPTION_POINTERS*)_exception_info
#define exception_info (struct _EXCEPTION_POINTERS*)_exception_info
#define AbnormalTermination _abnormal_termination
#define abnormal_termination _abnormal_termination
#define EXCEPTION_EXECUTE_HANDLER 1
#define EXCEPTION_CONTINUE_SEARCH 0
#define EXCEPTION_CONTINUE_EXECUTION (-1)
