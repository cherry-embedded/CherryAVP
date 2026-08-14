/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AVP_ERROR_H
#define AVP_ERROR_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AVP_OK = 0,
    AVP_EINVAL = -1,
    AVP_IO = -2,
    AVP_ENOMEM = -3,
    AVP_EUNSUPPORTED = -4,
    AVP_EBADHEADER = -5,
    AVP_ERANGE = -6,
    AVP_EBUFFER = -7,
    AVP_ENOENT = -8,
    AVP_ELACKFRAME = -9,
    AVP_EBADFRAME = -10
} avp_status_t;

#ifdef __cplusplus
}
#endif

#endif
