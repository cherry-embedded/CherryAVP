#ifndef __CONFIG_TYPES_H__
#define __CONFIG_TYPES_H__

/* these are filled in by configure or cmake*/
#define INCLUDE_INTTYPES_H 1
#define INCLUDE_STDINT_H 1
#define INCLUDE_SYS_TYPES_H 1

#if INCLUDE_INTTYPES_H
#  include <inttypes.h>
#endif
#if INCLUDE_STDINT_H
#  include <stdint.h>
#endif
#if INCLUDE_SYS_TYPES_H
#  include <sys/types.h>
#endif

typedef int16_t ogg_int16_t;
typedef uint16_t ogg_uint16_t;
typedef int32_t ogg_int32_t;
typedef uint32_t ogg_uint32_t;
typedef int64_t ogg_int64_t;
typedef uint64_t ogg_uint64_t;

#undef _ogg_malloc
#undef _ogg_calloc
#undef _ogg_realloc
#undef _ogg_free

#include "avp_common.h"

#define _ogg_malloc  avp_malloc
#define _ogg_calloc  avp_calloc
#define _ogg_realloc avp_realloc
#define _ogg_free    avp_free

#endif
