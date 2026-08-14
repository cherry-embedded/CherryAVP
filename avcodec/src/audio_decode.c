/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "audio_codec.h"

typedef audio_codec_dec_handle_t (*audio_codec_dec_driver_open_t)(const audio_codec_dec_config_t *config);
typedef void (*audio_codec_dec_driver_close_t)(audio_codec_dec_handle_t handle);
typedef avp_status_t (*audio_codec_dec_driver_frame_t)(audio_codec_dec_handle_t handle,
                                                       audio_codec_dec_in_frame_t *in_frame,
                                                       audio_codec_dec_out_frame_t *out_frame);

typedef struct {
    uint32_t codec_type;
    audio_codec_dec_driver_open_t open;
    audio_codec_dec_driver_close_t close;
    audio_codec_dec_driver_frame_t frame;
} audio_codec_dec_driver_t;

typedef struct {
    const audio_codec_dec_driver_t *driver;
    audio_codec_dec_handle_t decoder;
} audio_codec_dec_context_t;

#if defined(CONFIG_CHERRYAVP_AAC)
static audio_codec_dec_handle_t audio_codec_dec_open_aac(const audio_codec_dec_config_t *config)
{
    if (config == NULL) {
        return NULL;
    }

    return aac_pcm_decode_open(&config->aac_config);
}
#endif

#if defined(CONFIG_CHERRYAVP_AMR)
static audio_codec_dec_handle_t audio_codec_dec_open_amr(const audio_codec_dec_config_t *config)
{
    if (config == NULL) {
        return NULL;
    }

    return amr_pcm_decode_open(&config->amr_config);
}
#endif

#if defined(CONFIG_CHERRYAVP_FLAC)
static audio_codec_dec_handle_t audio_codec_dec_open_flac(const audio_codec_dec_config_t *config)
{
    if (config == NULL) {
        return NULL;
    }

    return flac_pcm_decode_open(&config->flac_config);
}
#endif

#if defined(CONFIG_CHERRYAVP_ALAC)
static audio_codec_dec_handle_t audio_codec_dec_open_alac(const audio_codec_dec_config_t *config)
{
    if (config == NULL) {
        return NULL;
    }

    return alac_pcm_decode_open(&config->alac_config);
}
#endif

#if defined(CONFIG_CHERRYAVP_MP3)
static audio_codec_dec_handle_t audio_codec_dec_open_mp3(const audio_codec_dec_config_t *config)
{
    (void)config;
    return mp3_pcm_decode_open();
}
#endif

#if defined(CONFIG_CHERRYAVP_OPUS)
static audio_codec_dec_handle_t audio_codec_dec_open_opus(const audio_codec_dec_config_t *config)
{
    if (config == NULL) {
        return NULL;
    }

    return opus_pcm_decode_open(&config->opus_config);
}
#endif

#if defined(CONFIG_CHERRYAVP_VORBIS)
static audio_codec_dec_handle_t audio_codec_dec_open_vorbis(const audio_codec_dec_config_t *config)
{
    if (config == NULL) {
        return NULL;
    }

    return vorbis_pcm_decode_open(&config->vorbis_config);
}
#endif

#if defined(CONFIG_CHERRYAVP_ADPCM)
static audio_codec_dec_handle_t audio_codec_dec_open_adpcm_ima(const audio_codec_dec_config_t *config)
{
    if (config == NULL) {
        return NULL;
    }

    return adpcm_pcm_decode_open(&config->adpcm_config);
}
#endif

#if defined(CONFIG_CHERRYAVP_G711)
static audio_codec_dec_handle_t audio_codec_dec_open_g711(const audio_codec_dec_config_t *config)
{
    if (config == NULL) {
        return NULL;
    }

    return g711_pcm_decode_open(&config->g711_config);
}
#endif

#if defined(CONFIG_CHERRYAVP_G722)
static audio_codec_dec_handle_t audio_codec_dec_open_g722(const audio_codec_dec_config_t *config)
{
    if (config == NULL) {
        return NULL;
    }

    return g722_pcm_decode_open(&config->g722_config);
}
#endif

static audio_codec_dec_handle_t audio_codec_dec_open_pcm(const audio_codec_dec_config_t *config)
{
    if (config == NULL) {
        return NULL;
    }
    return pcm_decode_open(&config->pcm_config);
}

static const audio_codec_dec_driver_t audio_codec_dec_drivers[] = {
#if defined(CONFIG_CHERRYAVP_AAC)
    { AUDIO_CODEC_ID_AAC, audio_codec_dec_open_aac, aac_pcm_decode_close, aac_pcm_decode_frame },
#endif
#if defined(CONFIG_CHERRYAVP_AMR)
    { AUDIO_CODEC_ID_AMR, audio_codec_dec_open_amr, amr_pcm_decode_close, amr_pcm_decode_frame },
#endif
#if defined(CONFIG_CHERRYAVP_FLAC)
    { AUDIO_CODEC_ID_FLAC, audio_codec_dec_open_flac, flac_pcm_decode_close, flac_pcm_decode_frame },
#endif
#if defined(CONFIG_CHERRYAVP_ALAC)
    { AUDIO_CODEC_ID_ALAC, audio_codec_dec_open_alac, alac_pcm_decode_close, alac_pcm_decode_frame },
#endif
#if defined(CONFIG_CHERRYAVP_MP3)
    { AUDIO_CODEC_ID_MP3, audio_codec_dec_open_mp3, mp3_pcm_decode_close, mp3_pcm_decode_frame },
#endif
#if defined(CONFIG_CHERRYAVP_OPUS)
    { AUDIO_CODEC_ID_OPUS, audio_codec_dec_open_opus, opus_pcm_decode_close, opus_pcm_decode_frame },
#endif
#if defined(CONFIG_CHERRYAVP_VORBIS)
    { AUDIO_CODEC_ID_VORBIS, audio_codec_dec_open_vorbis, vorbis_pcm_decode_close, vorbis_pcm_decode_frame },
#endif
#if defined(CONFIG_CHERRYAVP_ADPCM)
    { AUDIO_CODEC_ID_IMA_ADPCM, audio_codec_dec_open_adpcm_ima, adpcm_pcm_decode_close, adpcm_pcm_decode_frame },
#endif
#if defined(CONFIG_CHERRYAVP_G711)
    { AUDIO_CODEC_ID_G711_ALAW, audio_codec_dec_open_g711, g711_pcm_decode_close, g711_pcm_decode_frame },
    { AUDIO_CODEC_ID_G711_ULAW, audio_codec_dec_open_g711, g711_pcm_decode_close, g711_pcm_decode_frame },
#endif
#if defined(CONFIG_CHERRYAVP_G722)
    { AUDIO_CODEC_ID_G722, audio_codec_dec_open_g722, g722_pcm_decode_close, g722_pcm_decode_frame },
#endif
#if 1
    { AUDIO_CODEC_ID_PCM, audio_codec_dec_open_pcm, pcm_decode_close, pcm_decode_frame },
#endif
    { 0u, NULL, NULL, NULL }
};

static const audio_codec_dec_driver_t *audio_codec_dec_find_driver(uint32_t codec_type)
{
    uint32_t i;

    for (i = 0u; i < (uint32_t)(sizeof(audio_codec_dec_drivers) / sizeof(audio_codec_dec_drivers[0])); i++) {
        if (audio_codec_dec_drivers[i].codec_type == codec_type) {
            return &audio_codec_dec_drivers[i];
        }
    }

    return NULL;
}

audio_codec_dec_handle_t audio_codec_dec_open(const audio_codec_dec_config_t *config)
{
    const audio_codec_dec_driver_t *driver;
    audio_codec_dec_context_t *ctx;
    audio_codec_dec_handle_t decoder;

    if (config == NULL) {
        return NULL;
    }

    driver = audio_codec_dec_find_driver(config->codec_type);
    if (driver == NULL || driver->open == NULL || driver->close == NULL || driver->frame == NULL) {
        return NULL;
    }

    decoder = driver->open(config);
    if (decoder == NULL) {
        return NULL;
    }

    ctx = (audio_codec_dec_context_t *)avp_malloc(sizeof(*ctx));
    if (ctx == NULL) {
        driver->close(decoder);
        return NULL;
    }

    memset(ctx, 0, sizeof(*ctx));

    ctx->driver = driver;
    ctx->decoder = decoder;
    return (audio_codec_dec_handle_t)ctx;
}

void audio_codec_dec_close(audio_codec_dec_handle_t handle)
{
    audio_codec_dec_context_t *ctx = (audio_codec_dec_context_t *)handle;

    if (ctx == NULL) {
        return;
    }

    ctx->driver->close(ctx->decoder);
    avp_free(ctx);
}

avp_status_t audio_codec_dec_frame(audio_codec_dec_handle_t handle,
                                          audio_codec_dec_in_frame_t *in_frame,
                                          audio_codec_dec_out_frame_t *out_frame)
{
    audio_codec_dec_context_t *ctx = (audio_codec_dec_context_t *)handle;

    if (ctx == NULL || ctx->driver == NULL || ctx->driver->frame == NULL || ctx->decoder == NULL ||
        in_frame == NULL || out_frame == NULL ||
        in_frame->buffer == NULL || in_frame->size == 0u) {
        return AVP_EINVAL;
    }

    return ctx->driver->frame(ctx->decoder, in_frame, out_frame);
}
