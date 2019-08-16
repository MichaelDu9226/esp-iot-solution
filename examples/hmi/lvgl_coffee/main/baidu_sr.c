#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "esp_http_client.h"
#include "sdkconfig.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "board.h"
#include "http_stream.h"
#include "i2s_stream.h"
#include "wav_encoder.h"
#include "esp_peripherals.h"
#include "periph_button.h"
#include "periph_wifi.h"
#include "filter_resample.h"
#include "baidu_sr.h"

#include "freertos/FreeRTOS.h"
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "json_utils.h"
#include "audio_error.h"


#include "cJSON.h"

static const char *TAG = "BAIDU_SR";
//extern QueueHandle_t queue_sr_draw;

#define GOOGLE_SR_TASK_STACK (6*1024)

int cJSON_to_result(char *text)
{
    char *draw_file_name[] = {"苹果", "梨子", "香蕉", "樱桃", "桃子", "恐龙", "企鹅", "机器人", "狮子", "兔子"};
    char* json_result;
	cJSON *max_index, *json, *arrayItem, *item, *object;
	int i;
    json_result = (char*) malloc(64);
    memset(json_result, 0x0, 64);
    ESP_LOGI(TAG, "cJSON_to_result\n");
	json=cJSON_Parse(text);
	if (!json)
	{
		ESP_LOGI(TAG, "Error before: [%s]\n",cJSON_GetErrorPtr());
	}
	else
	{
		arrayItem = cJSON_GetObjectItem(json,"result");
		if(arrayItem!=NULL)
		{
            int size = cJSON_GetArraySize(arrayItem);
			ESP_LOGI(TAG, "cJSON_GetArraySize: size=%d\n",size);
            if(size > 0){
                cJSON * pSub = cJSON_GetArrayItem(arrayItem, 0);
                if(NULL == pSub ){return 0;} 
                memcpy(json_result, pSub->valuestring, strlen(pSub->valuestring));
                ESP_LOGI(TAG, "Result: [%s]\n", json_result);
                for(int i = 0; i < 9; i++){
                    int draw_file_name_index;
                    draw_file_name_index = strstr(json_result, draw_file_name[i]);
                    if(draw_file_name_index > 0){
                        ESP_LOGI(TAG, "********************\n");
                        ESP_LOGI(TAG, "draw_file_name_index: [%d]\n", i);
                        ESP_LOGI(TAG, "********************\n");
                        uint8_t file_index;
                        file_index = (uint8_t)i;
                        //xQueueSend(queue_sr_draw, &file_index, portMAX_DELAY);
                    }
                }
            }
		}else{
            ESP_LOGI(TAG, "item = NULL\n");
        }
		cJSON_Delete(json);
	}
	return 0;
}

esp_err_t _http_stream_event_handle(http_stream_event_msg_t *msg)
{
    esp_http_client_handle_t http = (esp_http_client_handle_t)msg->http_client;
    baidu_sr_t *sr = (baidu_sr_t *)msg->user_data;

    char len_buf[16];
    static int total_write = 0;

    if (msg->event_id == HTTP_STREAM_PRE_REQUEST) {
        // set header
        ESP_LOGI(TAG, "[ + ] HTTP client HTTP_STREAM_PRE_REQUEST, lenght=%d", msg->buffer_len);
        //esp_http_client_set_header(http, "x-audio-sample-rates", "16000");
        //esp_http_client_set_header(http, "x-audio-bits", "16");
        //esp_http_client_set_header(http, "x-audio-channel", "1");
        esp_http_client_set_header(http, "Content-Type", "audio/pcm;rate=16000");
        total_write = 0;
        return ESP_OK;
    }

    if (msg->event_id == HTTP_STREAM_ON_REQUEST) {
        // write data
        int wlen = sprintf(len_buf, "%x\r\n", msg->buffer_len);
        if (esp_http_client_write(http, len_buf, wlen) <= 0) {
            return ESP_FAIL;
        }
        if (esp_http_client_write(http, msg->buffer, msg->buffer_len) <= 0) {
            return ESP_FAIL;
        }
        if (esp_http_client_write(http, "\r\n", 2) <= 0) {
            return ESP_FAIL;
        }
        total_write += msg->buffer_len;
        //printf("\033[A\33[2K\rTotal bytes written: %d\n", total_write);
        return msg->buffer_len;
    }

    if (msg->event_id == HTTP_STREAM_POST_REQUEST) {
        //ESP_LOGI(TAG, "[ + ] HTTP client HTTP_STREAM_POST_REQUEST, write end chunked marker");
        if (esp_http_client_write(http, "0\r\n\r\n", 5) <= 0) {
            return ESP_FAIL;
        }
        return ESP_OK;
    }

    if (msg->event_id == HTTP_STREAM_FINISH_REQUEST) {
        //ESP_LOGI(TAG, "[ + ] HTTP client HTTP_STREAM_FINISH_REQUEST");
        char *buf = calloc(1, 2048);
        assert(buf);         
        int read_len = esp_http_client_read(http, buf, 2048);
        //ESP_LOGI(TAG, "[ + ] HTTP client HTTP_STREAM_FINISH_REQUEST, read_len=%d", read_len);
        if (read_len <= 0) {
            ESP_LOGI(TAG, "read_len <= 0");
            free(buf);
            return ESP_FAIL;
        }
        buf[read_len] = 0;
        ESP_LOGI(TAG, "Got HTTP Response = %s", (char *)buf);
        ESP_LOGI(TAG, "baidu_sr sample_rates:%d", sr->sample_rates);
        if (sr->response_text) {
            free(sr->response_text);
        }
        cJSON_to_result(buf);
        sr->response_text = json_get_token_value(buf, "result");
        free(buf);
        return ESP_OK;        
    }
    return ESP_OK;
}

baidu_sr_handle_t baidu_sr_init(baidu_sr_config_t *config)
{
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    baidu_sr_t *sr = calloc(1, sizeof(baidu_sr_t));
    AUDIO_MEM_CHECK(TAG, sr, return NULL);
    sr->pipeline = audio_pipeline_init(&pipeline_cfg);

    sr->buffer_size = config->buffer_size;
    if (sr->buffer_size <= 0) {
        sr->buffer_size = DEFAULT_SR_BUFFER_SIZE;
    }

    sr->buffer = malloc(sr->buffer_size);
    AUDIO_MEM_CHECK(TAG, sr->buffer, goto exit_sr_init);
    sr->b64_buffer = malloc(sr->buffer_size);
    AUDIO_MEM_CHECK(TAG, sr->b64_buffer, goto exit_sr_init);
    sr->lang_code = strdup(config->lang_code);
    AUDIO_MEM_CHECK(TAG, sr->lang_code, goto exit_sr_init);
    sr->api_key = strdup(config->api_key);
    AUDIO_MEM_CHECK(TAG, sr->api_key, goto exit_sr_init);

    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_READER;
    sr->i2s_stream_reader = i2s_stream_init(&i2s_cfg);
/*
    http_stream_cfg_t http_cfg = {
        .type = AUDIO_STREAM_WRITER,
        .event_handle = _http_stream_writer_event_handle,
        .user_data = sr,
        .task_stack = GOOGLE_SR_TASK_STACK,
    };
*/
    //http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
    //http_cfg.type = AUDIO_STREAM_WRITER;
    //http_cfg.event_handle = _http_stream_event_handle;
     http_stream_cfg_t http_cfg = {
        .type = AUDIO_STREAM_WRITER,
        .event_handle = _http_stream_event_handle,
        .user_data = sr,
        .task_stack = GOOGLE_SR_TASK_STACK,
    };

    sr->http_stream_writer = http_stream_init(&http_cfg);
    sr->sample_rates = config->record_sample_rates;
    //sr->encoding = config->encoding;
    //sr->on_begin = config->on_begin;

    audio_pipeline_register(sr->pipeline, sr->http_stream_writer, "sr_http");
    audio_pipeline_register(sr->pipeline, sr->i2s_stream_reader,         "sr_i2s");
    audio_pipeline_link(sr->pipeline, (const char *[]) {"sr_i2s", "sr_http"}, 2);
    //i2s_stream_set_clk(sr->i2s_reader, config->record_sample_rates, 16, 1);
    i2s_stream_set_clk(sr->i2s_stream_reader, config->record_sample_rates, 16, 1);

    return sr;
exit_sr_init:
    baidu_sr_destroy(sr);
    return NULL;
}

esp_err_t baidu_sr_destroy(baidu_sr_handle_t sr)
{
    if (sr == NULL) {
        return ESP_FAIL;
    }
    audio_pipeline_terminate(sr->pipeline);
    audio_pipeline_remove_listener(sr->pipeline);
    audio_pipeline_deinit(sr->pipeline);
    audio_element_deinit(sr->i2s_stream_reader);
    audio_element_deinit(sr->http_stream_writer);
    free(sr->buffer);
    free(sr->b64_buffer);
    free(sr->lang_code);
    free(sr->api_key);
    free(sr);
    return ESP_OK;
}

esp_err_t baidu_sr_set_listener(baidu_sr_handle_t sr, audio_event_iface_handle_t listener)
{
    if (listener) {
        audio_pipeline_set_listener(sr->pipeline, listener);
    }
    return ESP_OK;
}

esp_err_t baidu_sr_start(baidu_sr_handle_t sr)
{
    audio_element_set_uri(sr->http_stream_writer, "http://vop.baidu.com/server_api?dev_pid=1536&cuid=123456PHP&token=24.37dbc9549804cd1abceb157fbed7b979.2592000.1567079218.282335-16199280");
    //audio_element_set_uri(sr->http_stream_writer, "http://vop.baidu.com/server_api?dev_pid=1536&cuid=123456PHP&token=24.c80edf5619e9dbefd1f6f0059174ee34.2592000.1564067747.282335-15803531");      
    audio_pipeline_reset_items_state(sr->pipeline);
    audio_pipeline_reset_ringbuffer(sr->pipeline);
    audio_pipeline_run(sr->pipeline);
    return ESP_OK;
}

char *baidu_sr_stop(baidu_sr_handle_t sr)
{
    audio_pipeline_stop(sr->pipeline);
    audio_pipeline_wait_for_stop(sr->pipeline);
    return sr->response_text;
}