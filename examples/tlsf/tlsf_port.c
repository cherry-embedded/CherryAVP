#include "tlsf.h"
#include "tlsf_port.h"

#ifndef CONFIG_TLSF_MEM_POOL_SIZE
#define CONFIG_TLSF_MEM_POOL_SIZE (20 * 1024 * 1024)
#endif

tlsf_t g_tlsf_mem;

uint8_t tlsf_mem_pool[CONFIG_TLSF_MEM_POOL_SIZE];

void avp_mem_init(void)
{
    g_tlsf_mem = tlsf_create_with_pool(tlsf_mem_pool, sizeof(tlsf_mem_pool));
}

void *avp_malloc_dbg(size_t size, const char *file, const char *func, int line)
{
    size_t free_before = tlsf_free_size(g_tlsf_mem);
    void *ptr = tlsf_malloc(g_tlsf_mem, size);
    printf("avp_malloc: ptr=%p, size=%u, free=%u->%u, at %s:%d %s()\n",
           ptr,
           (unsigned)size,
           (unsigned)free_before,
           (unsigned)tlsf_free_size(g_tlsf_mem),
           file,
           line,
           func);
    return ptr;
}

void *avp_calloc_dbg(size_t nmemb, size_t size, const char *file, const char *func, int line)
{
    size_t total = nmemb * size;
    void *p = avp_malloc_dbg(total, file, func, line);

    printf("avp_calloc: ptr=%p, nmemb=%u, size=%u, total=%u, at %s:%d %s()\n",
           p,
           (unsigned)nmemb,
           (unsigned)size,
           (unsigned)total,
           file,
           line,
           func);
    if (p != NULL) {
        memset(p, 0, total);
    }
    return p;
}

void avp_free_dbg(void *ptr, const char *file, const char *func, int line)
{
    size_t free_before = tlsf_free_size(g_tlsf_mem);

    tlsf_free(g_tlsf_mem, ptr);
    printf("avp_free: ptr=%p, free=%u->%u, at %s:%d %s()\n",
           ptr,
           (unsigned)free_before,
           (unsigned)tlsf_free_size(g_tlsf_mem),
           file,
           line,
           func);
}

void *avp_realloc_dbg(void *ptr, size_t size, const char *file, const char *func, int line)
{
    size_t free_before = tlsf_free_size(g_tlsf_mem);
    void *new_ptr = tlsf_realloc(g_tlsf_mem, ptr, size);

    printf("avp_realloc: ptr=%p->%p, size=%u, free=%u->%u, at %s:%d %s()\n",
           ptr,
           new_ptr,
           (unsigned)size,
           (unsigned)free_before,
           (unsigned)tlsf_free_size(g_tlsf_mem),
           file,
           line,
           func);
    return new_ptr;
}

void *avp_malloc(size_t size)
{
    return tlsf_malloc(g_tlsf_mem, size);
}

void *avp_calloc(size_t nmemb, size_t size)
{
    void *p = tlsf_malloc(g_tlsf_mem, nmemb * size);
    if (p != NULL) {
        memset(p, 0, nmemb * size);
    }
    return p;
}

void avp_free(void *ptr)
{
    tlsf_free(g_tlsf_mem, ptr);
}

void *avp_realloc(void *ptr, size_t size)
{
    return tlsf_realloc(g_tlsf_mem, ptr, size);
}

uint32_t avp_mem_free_size(void)
{
    return (uint32_t)tlsf_free_size(g_tlsf_mem);
}