/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AVP_ALLOC_H
#define AVP_ALLOC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_AVP_DEBUG_MEMORY
void *avp_malloc_dbg(size_t size, const char *file, const char *func, int line);
void *avp_calloc_dbg(size_t nmemb, size_t size, const char *file, const char *func, int line);
void *avp_realloc_dbg(void *ptr, size_t size, const char *file, const char *func, int line);
void avp_free_dbg(void *ptr, const char *file, const char *func, int line);

#define avp_malloc(size)        avp_malloc_dbg((size), __FILE__, __func__, __LINE__)
#define avp_calloc(nmemb, size) avp_calloc_dbg((nmemb), (size), __FILE__, __func__, __LINE__)
#define avp_realloc(ptr, size)  avp_realloc_dbg((ptr), (size), __FILE__, __func__, __LINE__)
#define avp_free(ptr)           avp_free_dbg((ptr), __FILE__, __func__, __LINE__)
#else
void *avp_malloc(size_t size);
void *avp_calloc(size_t nmemb, size_t size);
void *avp_realloc(void *ptr, size_t size);
void avp_free(void *ptr);
#endif
#ifdef __cplusplus
}
#endif

#endif