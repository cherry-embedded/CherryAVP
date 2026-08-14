#ifndef CUSTOM_SUPPORT_H
#define CUSTOM_SUPPORT_H

#include "avp_common.h"

#define OVERRIDE_OPUS_ALLOC
#define OVERRIDE_OPUS_REALLOC
#define OVERRIDE_OPUS_FREE

#define opus_alloc avp_malloc
#define opus_realloc avp_realloc
#define opus_free avp_free

#endif