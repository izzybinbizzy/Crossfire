#pragma once
#include <vcruntime.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum _crt_argv_mode { _crt_argv_no_arguments, _crt_argv_unexpanded_arguments, _crt_argv_expanded_arguments } _crt_argv_mode;
typedef enum _crt_exit_return_mode { _crt_exit_terminate_process, _crt_exit_return_to_caller } _crt_exit_return_mode;
typedef enum _crt_exit_cleanup_mode { _crt_exit_full_cleanup, _crt_exit_quick_cleanup, _crt_exit_no_cleanup } _crt_exit_cleanup_mode;
typedef void (__cdecl* _PVFV)(void);
typedef int (__cdecl* _PIFV)(void);
typedef void (__cdecl* _PVFI)(int);
#ifdef __cplusplus
}
#endif
