/* ------------------------------------------------------------------
 * Copyright (C) 2009 Martin Storsjo
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 * -------------------------------------------------------------------
 */

#ifndef OSCL_MEM_H
#define OSCL_MEM_H
#ifndef OSCL_MEM_H_INCLUDED
#define OSCL_MEM_H_INCLUDED
#endif

#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
#include <new>
#endif

#include "avp_common.h"

#define oscl_malloc avp_malloc
#define oscl_free avp_free
#define oscl_memset memset
#define oscl_memmove memmove
#define oscl_memcpy memcpy
#define oscl_memcmp memcmp

#ifdef __cplusplus
#define OSCL_NEW(T, params) new (std::nothrow) T params
#define OSCL_DELETE(ptr) delete ptr
#define OSCL_ARRAY_NEW(T, count) new (std::nothrow) T[count]
#define OSCL_ARRAY_DELETE(ptr) delete [] ptr
#else
#define OSCL_ARRAY_NEW(T, count) ((T *)oscl_malloc(sizeof(T) * (count)))
#define OSCL_ARRAY_DELETE(ptr) oscl_free(ptr)
#endif

#endif
