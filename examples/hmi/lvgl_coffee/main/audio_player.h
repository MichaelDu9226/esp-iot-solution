#ifndef _AUDIO_PLAYER_H_
#define _AUDIO_PLAYER_H_

#include "esp_err.h"
#include "audio_event_iface.h"
#include "audio_element.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct audio_player {
    audio_pipeline_handle_t pipeline;
    audio_element_handle_t  rsp_handle;
    audio_element_handle_t  mp3_decoder;
    audio_element_handle_t  i2s_stream_writer;
    audio_element_handle_t  http_stream_writer;
} audio_player_t;

typedef struct {
    const char *api_key;                /*!< API Key */
    const char *lang_code;              /*!< Speech-to-Text language code */
    int record_sample_rates;            /*!< Audio recording sample rate */
    //google_sr_encoding_t encoding;      /*!< Audio encoding */
    int buffer_size;                    /*!< Processing buffer size */
    //baidu_sr_event_handle_t on_begin;  /*!< Begin send audio data to server */
} audio_player_config_t;

typedef struct audio_player* audio_player_handle_t;
typedef void (*audio_player_event_handle_t)(audio_player_handle_t player);

audio_player_handle_t audio_player_init(audio_player_config_t *config);

esp_err_t audio_player_start(audio_player_handle_t player);

void audio_player_stop(audio_player_handle_t player);

esp_err_t audio_player_destroy(audio_player_handle_t player);

esp_err_t audio_player_set_listener(audio_player_handle_t player, audio_event_iface_handle_t listener);

void audio_player_play(audio_player_handle_t player, char* mp3name);

#ifdef __cplusplus
}
#endif

#endif