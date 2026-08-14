/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AVP_AE_VOL_H
#define AVP_AE_VOL_H

#include "avp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Number of volume control steps. */
#define AVP_AE_VOL_STEPS 256u

/** @brief Default minimum gain in dB for the volume curve. */
#define AVP_AE_VOL_DEFAULT_MIN_DB (-60)

/** @brief Default maximum gain in dB for the volume curve. */
#define AVP_AE_VOL_DEFAULT_MAX_DB 18

/** @brief Volume effect runtime control commands. */
typedef enum {
    AVP_AE_VOL_CMD_SET_INDEX = 0, /**< Set volume index, range 0..255 (uint8_t*). */
    AVP_AE_VOL_CMD_GET_INDEX,     /**< Get current volume index (uint8_t*). */
    AVP_AE_VOL_CMD_SET_DB_RANGE,  /**< Set dB mapping range (avp_ae_vol_db_range_t*). */
    AVP_AE_VOL_CMD_GET_DB_RANGE,  /**< Get dB mapping range (avp_ae_vol_db_range_t*). */
    AVP_AE_VOL_CMD_SET_ENABLE,    /**< Enable or bypass processing (int*; 0/1). */
    AVP_AE_VOL_CMD_GET_ENABLE,    /**< Get enable flag (int*). */
    AVP_AE_VOL_CMD_GET_GAIN_Q14,  /**< Get current Q14 linear gain factor (int32_t*). */
} avp_ae_vol_cmd_t;

/** @brief dB range used to map volume index values to linear gain. */
typedef struct {
    int min_db; /**< Gain in dB for the first non-zero index. */
    int max_db; /**< Gain in dB for index 255. */
} avp_ae_vol_db_range_t;

/** @brief Volume effect instance configuration. */
typedef struct {
    int min_db;     /**< Minimum gain in dB. If min_db and max_db are both 0, defaults are used. */
    int max_db;     /**< Maximum gain in dB. If min_db and max_db are both 0, defaults are used. */
    uint8_t index;  /**< Initial volume index; 0 means mute. */
    uint8_t enable; /**< Non-zero to process audio, zero to bypass. */
} avp_ae_vol_config_t;

typedef struct avp_ae_vol avp_ae_vol_t;

/**
 * @brief Create a volume effect instance.
 *
 * The processor uses a 256-step gain table. Index 0 is always mute. Indices
 * 1..255 are distributed across @p min_db to @p max_db and converted to Q14
 * linear gain factors. Input and output buffers may alias each other.
 *
 * @param[in]  config  Volume effect configuration.
 * @param[out] handle  Receives the newly created volume effect instance.
 * @return @ref AVP_OK on success, or a negative error code on failure.
 */
avp_status_t avp_ae_vol_open(const avp_ae_vol_config_t *config,
                             avp_ae_vol_t **handle);

/**
 * @brief Release all resources owned by the volume effect instance.
 *
 * @param[in] handle  Volume effect instance; NULL is accepted.
 */
void avp_ae_vol_close(avp_ae_vol_t *handle);

/**
 * @brief Change or query volume effect parameters at runtime.
 *
 * @param[in]     handle  Volume effect instance.
 * @param[in]     cmd     Control command; see @ref avp_ae_vol_cmd_t.
 * @param[in,out] arg     Command argument; type depends on @p cmd.
 * @return @ref AVP_OK on success, or a negative error code on failure.
 */
avp_status_t avp_ae_vol_control(avp_ae_vol_t *handle,
                                avp_ae_vol_cmd_t cmd,
                                void *arg);

/**
 * @brief Apply volume scaling to signed 16-bit PCM samples.
 *
 * @p sample_count is the total number of @c int16_t values, including all
 * interleaved channels. For example, 480 stereo frames contain 960 samples.
 *
 * @param[in]  handle        Volume effect instance.
 * @param[in]  in            Input PCM buffer.
 * @param[out] out           Output PCM buffer; may be the same as @p in.
 * @param[in]  sample_count  Number of int16 samples to process.
 * @return @ref AVP_OK on success, or a negative error code on failure.
 */
avp_status_t avp_ae_vol_process(avp_ae_vol_t *handle,
                                const int16_t *in,
                                int16_t *out,
                                uint32_t sample_count);

#ifdef __cplusplus
}
#endif

#endif
