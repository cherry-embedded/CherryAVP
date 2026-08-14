#include <cstdlib>
#include <new>
#include "avp_common.h"

void* operator new(size_t size) noexcept {
    void* ptr = avp_malloc(size);
    return ptr;
}
void operator delete(void* ptr) noexcept {
    avp_free(ptr);
}
void* operator new[](size_t size) noexcept { return avp_malloc(size); }
void operator delete[](void* ptr) noexcept { avp_free(ptr); }