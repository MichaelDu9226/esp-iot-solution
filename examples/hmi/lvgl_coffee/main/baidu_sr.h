#ifndef _BAIDU_SR_H_
#define _BAIDU_SR_H_

#include "esp_err.h"
#include "audio_event_iface.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_SR_BUFFER_SIZE (2048)

typedef struct baidu_sr {
    audio_pipeline_handle_t pipeline;
    int                     remain_len;
    int                     sr_total_write;
    bool                    is_begin;
    char                    *buffer;
    char                    *b64_buffer;
    audio_element_handle_t  i2s_stream_reader;
    audio_element_handle_t  http_stream_writer;
    char                    *lang_code;
    char                    *api_key;
    int                     sample_rates;
    int                     buffer_size;
    //baidu_sr_encoding_t    encoding;
    char                    *response_text;
    //google_sr_event_handle_t on_begin;
} baidu_sr_t;

typedef struct baidu_sr* baidu_sr_handle_t;
typedef void (*baidu_sr_event_handle_t)(baidu_sr_handle_t sr);

/**
 * Google Cloud Speech-to-Text configurations
 */
typedef struct {
    const char *api_key;                /*!< API Key */
    const char *lang_code;              /*!< Speech-to-Text language code */
    int record_sample_rates;            /*!< Audio recording sample rate */
    //google_sr_encoding_t encoding;      /*!< Audio encoding */
    int buffer_size;                    /*!< Processing buffer size */
    //baidu_sr_event_handle_t on_begin;  /*!< Begin send audio data to server */
} baidu_sr_config_t;

baidu_sr_handle_t baidu_sr_init(baidu_sr_config_t *config);

esp_err_t baidu_sr_start(baidu_sr_handle_t sr);

char *baidu_sr_stop(baidu_sr_handle_t sr);

esp_err_t baidu_sr_destroy(baidu_sr_handle_t sr);

esp_err_t baidu_sr_set_listener(baidu_sr_handle_t sr, audio_event_iface_handle_t listener);

#ifdef __cplusplus
}
#endif

#endif


