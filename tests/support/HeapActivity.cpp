#include "HeapActivity.h"
#include <cstdlib>
#include <new>
#if defined(_MSC_VER)
#include <malloc.h>
#endif
namespace testheap { thread_local Counts* active=nullptr; }
#if defined(SYNTH2_TEST_WRAP_C_HEAP)
extern "C" void* __real_malloc(std::size_t);
extern "C" void* __real_calloc(std::size_t,std::size_t);
extern "C" void* __real_realloc(void*,std::size_t);
extern "C" void __real_free(void*);
extern "C" void* __wrap_malloc(std::size_t n) {
    if(testheap::active) ++testheap::active->cAllocations;
    return __real_malloc(n);
}
extern "C" void* __wrap_calloc(std::size_t n,std::size_t size) {
    if(testheap::active) ++testheap::active->cAllocations;
    return __real_calloc(n,size);
}
extern "C" void* __wrap_realloc(void* p,std::size_t n) {
    if(testheap::active) ++testheap::active->cAllocations;
    return __real_realloc(p,n);
}
extern "C" void __wrap_free(void* p) {
    if(p && testheap::active) ++testheap::active->cDeallocations;
    __real_free(p);
}
#endif
namespace
{
void* allocate(std::size_t size)
{
    if(testheap::active) ++testheap::active->allocations;
#if defined(SYNTH2_TEST_WRAP_C_HEAP)
    auto* pointer=__real_malloc(size ? size : 1);
#else
    auto* pointer=std::malloc(size ? size : 1);
#endif
    if(pointer) return pointer;
    throw std::bad_alloc();
}
void dispose(void* pointer) noexcept
{
    if(pointer && testheap::active) ++testheap::active->deallocations;
#if defined(SYNTH2_TEST_WRAP_C_HEAP)
    __real_free(pointer);
#else
    std::free(pointer);
#endif
}
void* alignedAllocate(std::size_t size,std::size_t alignment)
{
    if(testheap::active) ++testheap::active->allocations;
#if defined(_MSC_VER)
    auto* pointer=_aligned_malloc(size ? size : alignment,alignment);
#else
    const auto rounded=((size ? size : 1)+alignment-1)/alignment*alignment;
    auto* pointer=std::aligned_alloc(alignment,rounded);
#endif
    if(pointer) return pointer;
    throw std::bad_alloc();
}
void alignedDispose(void* pointer) noexcept
{
    if(pointer && testheap::active) ++testheap::active->deallocations;
#if defined(_MSC_VER)
    _aligned_free(pointer);
#else
#if defined(SYNTH2_TEST_WRAP_C_HEAP)
    __real_free(pointer);
#else
    std::free(pointer);
#endif
#endif
}
}
void* operator new(std::size_t size) { return allocate(size); }
void* operator new[](std::size_t size) { return allocate(size); }
void operator delete(void* p) noexcept { dispose(p); }
void operator delete[](void* p) noexcept { dispose(p); }
void operator delete(void* p,std::size_t) noexcept { dispose(p); }
void operator delete[](void* p,std::size_t) noexcept { dispose(p); }
void* operator new(std::size_t n,const std::nothrow_t&) noexcept { try{return allocate(n);}catch(...){return nullptr;} }
void* operator new[](std::size_t n,const std::nothrow_t& tag) noexcept { return ::operator new(n,tag); }
void operator delete(void* p,const std::nothrow_t&) noexcept { dispose(p); }
void operator delete[](void* p,const std::nothrow_t&) noexcept { dispose(p); }
void* operator new(std::size_t n,std::align_val_t a) { return alignedAllocate(n,static_cast<std::size_t>(a)); }
void* operator new[](std::size_t n,std::align_val_t a) { return alignedAllocate(n,static_cast<std::size_t>(a)); }
void operator delete(void* p,std::align_val_t) noexcept { alignedDispose(p); }
void operator delete[](void* p,std::align_val_t) noexcept { alignedDispose(p); }
void operator delete(void* p,std::size_t,std::align_val_t) noexcept { alignedDispose(p); }
void operator delete[](void* p,std::size_t,std::align_val_t) noexcept { alignedDispose(p); }
void* operator new(std::size_t n,std::align_val_t a,const std::nothrow_t&) noexcept {try{return alignedAllocate(n,static_cast<std::size_t>(a));}catch(...){return nullptr;}}
void* operator new[](std::size_t n,std::align_val_t a,const std::nothrow_t& t) noexcept {return ::operator new(n,a,t);}
void operator delete(void* p,std::align_val_t,const std::nothrow_t&) noexcept {alignedDispose(p);}
void operator delete[](void* p,std::align_val_t,const std::nothrow_t&) noexcept {alignedDispose(p);}
