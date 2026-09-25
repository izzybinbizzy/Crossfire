#pragma once
#include <vcruntime.h>
#pragma pack(push, _CRT_PACKING)
#ifdef __cplusplus
extern "C++" {
namespace std {
enum class align_val_t : size_t {};
struct nothrow_t { explicit nothrow_t() = default; };
extern nothrow_t const nothrow;
}
#define __NOTHROW_T_DEFINED
[[nodiscard]] __declspec(allocator) void* __cdecl operator new(size_t _Size);
[[nodiscard]] __declspec(allocator) void* __cdecl operator new(size_t _Size, ::std::nothrow_t const&) noexcept;
[[nodiscard]] __declspec(allocator) void* __cdecl operator new[](size_t _Size);
[[nodiscard]] __declspec(allocator) void* __cdecl operator new[](size_t _Size, ::std::nothrow_t const&) noexcept;
void __cdecl operator delete(void* _Block) noexcept;
void __cdecl operator delete(void* _Block, ::std::nothrow_t const&) noexcept;
void __cdecl operator delete[](void* _Block) noexcept;
void __cdecl operator delete[](void* _Block, ::std::nothrow_t const&) noexcept;
void __cdecl operator delete(void* _Block, size_t _Size) noexcept;
void __cdecl operator delete[](void* _Block, size_t _Size) noexcept;
[[nodiscard]] __declspec(allocator) void* __cdecl operator new(size_t _Size, ::std::align_val_t _Al);
[[nodiscard]] __declspec(allocator) void* __cdecl operator new(size_t _Size, ::std::align_val_t _Al, ::std::nothrow_t const&) noexcept;
[[nodiscard]] __declspec(allocator) void* __cdecl operator new[](size_t _Size, ::std::align_val_t _Al);
[[nodiscard]] __declspec(allocator) void* __cdecl operator new[](size_t _Size, ::std::align_val_t _Al, ::std::nothrow_t const&) noexcept;
void __cdecl operator delete(void* _Block, ::std::align_val_t _Al) noexcept;
void __cdecl operator delete(void* _Block, ::std::align_val_t _Al, ::std::nothrow_t const&) noexcept;
void __cdecl operator delete[](void* _Block, ::std::align_val_t _Al) noexcept;
void __cdecl operator delete[](void* _Block, ::std::align_val_t _Al, ::std::nothrow_t const&) noexcept;
void __cdecl operator delete(void* _Block, size_t _Size, ::std::align_val_t _Al) noexcept;
void __cdecl operator delete[](void* _Block, size_t _Size, ::std::align_val_t _Al) noexcept;
#define __PLACEMENT_NEW_INLINE
[[nodiscard]] inline void* __cdecl operator new(size_t _Size, void* _Where) noexcept { (void)_Size; return _Where; }
inline void __cdecl operator delete(void*, void*) noexcept {}
#define __PLACEMENT_VEC_NEW_INLINE
[[nodiscard]] inline void* __cdecl operator new[](size_t _Size, void* _Where) noexcept { (void)_Size; return _Where; }
inline void __cdecl operator delete[](void*, void*) noexcept {}
}
#endif
#pragma pack(pop)
