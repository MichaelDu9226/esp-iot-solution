/***************************/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "nvs_flash.h"

#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "fatfs_stream.h"
#include "i2s_stream.h"
#include "mp3_decoder.h"
#include "filter_resample.h"

#include "esp_peripherals.h"
#include "periph_sdcard.h"
#include "periph_touch.h"
#include "input_key_service.h"
#include "periph_adc_button.h"
#include "board.h"
#include "audio_player.h"


#define TAG "player"

/**********************
 *      MACROS
 **********************/
#define CONTROL_CURRENT -1
#define CONTROL_NEXT -2
#define CONTROL_PREV -3
#define MAX_PLAY_FILE_NUM 20
//#define CURRENT 0
//#define NEXT    1

/* Control with a touch pad playing MP3 files from SD Card

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
typedef enum {
    BUTTON_LEFT = 0,
    BUTTON_RIGHT,
    BUTTON_OK,
    BUTTON_RETURN,
} button_state_t;

// more files may be added and `MP3_FILE_COUNT` will reflect the actual count
#define MP3_FILE_COUNT sizeof(mp3_file)/sizeof(char*)

#define CURRENT 0
#define NEXT    1

char current_mp3_name[32];

static FILE *get_file(int next_file)
{
    static FILE *file;
    static int file_index = 0;

    if (next_file != CURRENT) {
        if (file != NULL) {
            fclose(file);
            file = NULL;
        }
    }
    // return a handle to the current file
    if (file == NULL) {
        file = fopen(current_mp3_name, "r");
        if (!file) {
            ESP_LOGE(TAG, "Error opening file");
            return NULL;
        }
    }
    return file;
}

void audio_player_play(audio_player_handle_t player, char* mp3name)
{
	assert(mp3name != 0);
	strcpy(current_mp3_name, mp3name); 
	ESP_LOGI(TAG, "mp3 file name: %s", current_mp3_name);
	audio_pipeline_terminate(player->pipeline);
	get_file(NEXT);	
	audio_pipeline_run(player->pipeline);	
}


/*
 * Callback function to feed audio data stream from sdcard to mp3 decoder element
 */
static int my_sdcard_read_cb(audio_element_handle_t el, char *buf, int len, TickType_t wait_time, void *ctx)
{
    int read_len = fread(buf, 1, len, get_file(CURRENT));
    if (read_len == 0) {
        read_len = AEL_IO_DONE;
    }
    return read_len;
}
/*
static esp_err_t input_key_service_cb(periph_service_handle_t handle, periph_service_event_t *evt, void *ctx)
{

    audio_board_handle_t board_handle = (audio_board_handle_t) ctx;
    int player_volume;
	uint8_t button_state;
    audio_hal_get_volume(board_handle->audio_hal, &player_volume);

    if (evt->type == INPUT_KEY_SERVICE_ACTION_CLICK_RELEASE) {
        switch ((int)evt->data) {
			case USER_ID_REC://LEFT
                ESP_LOGI(TAG, "[ * ] [LEFT] key");
				assert(event_queue != 0);
				button_state = 0;
				xQueueSend(event_queue, &button_state, portMAX_DELAY);		
                break;
						
            case USER_ID_PLAY: //Right
                ESP_LOGI(TAG, "[ * ] [Right] key");
				assert(event_queue != 0);
				button_state = 1;
				xQueueSend(event_queue, &button_state, portMAX_DELAY);
				
                break;

			case USER_ID_VOLDOWN://OK
                ESP_LOGI(TAG, "[ * ] [OK] key");
				assert(event_queue != 0);
				button_state = 2;
				xQueueSend(event_queue, &button_state, portMAX_DELAY);
                break;

			case USER_ID_MODE://RETURN
                ESP_LOGI(TAG, "[ * ] [RETURN] key");
				assert(event_queue != 0);
				button_state = 3;
				xQueueSend(event_queue, &button_state, portMAX_DELAY);
                break;
			
            case USER_ID_SET:
                ESP_LOGI(TAG, "[ * ] [Set] input key event");
                break;
				
            case USER_ID_VOLUP:
                ESP_LOGI(TAG, "[ * ] [Vol+] input key event");
                break;
        }
    }

    return ESP_OK;
}
*/

extern void Es8388ReadAll();
audio_player_handle_t audio_player_init(audio_player_config_t *config)
{
    //audio_element_handle_t rsp_handle;
/*
    if (button_queue == NULL) {
        button_queue = xQueueCreate(10, sizeof(uint8_t));
    }
*/
    ESP_LOGI(TAG, "[1.0] Initialize peripherals management");
    esp_periph_config_t periph_cfg = {\
    .task_stack         = 4096,   \
    .task_prio          = 5,    \
    .task_core          = 1,    \
};
    //DEFAULT_ESP_PHERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    periph_sdcard_cfg_t sdcard_cfg = {
        .root = "/sdcard",
        .card_detect_pin = get_sdcard_intr_gpio(), //GPIO_NUM_34
    };
    esp_periph_handle_t sdcard_handle = periph_sdcard_init(&sdcard_cfg);
    ESP_LOGI(TAG, "[1.1] Start SD card peripheral");
    esp_periph_start(set, sdcard_handle);

    // Wait until sdcard is mounted
    while (!periph_sdcard_is_mounted(sdcard_handle)) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
	

	//strcpy(current_mp3_name, "/sdcard/start.mp3");
    strcpy(current_mp3_name, "/sdcard/winxp.mp3");
    //strcpy(current_mp3_name, "/sdcard/win.mp3");

    ESP_LOGI(TAG, "[1.2] Initialize and start peripherals");

    periph_adc_button_cfg_t adc_btn_cfg = {0};
    adc_arr_t adc_btn_tag = ADC_DEFAULT_ARR();
    adc_btn_cfg.arr = &adc_btn_tag;
    adc_btn_cfg.arr_size = 1;
    esp_periph_handle_t adc_btn_handle = periph_adc_button_init(&adc_btn_cfg);
    //esp_periph_start(set, adc_btn_handle);

    ESP_LOGI(TAG, "[ 2 ] Start codec chip");
    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);

    ESP_LOGI(TAG, "[ 3 ] Create and start input key service");
    //input_key_service_info_t input_key_info[] = INPUT_KEY_DEFAULT_INFO();
    //periph_service_handle_t input_ser = input_key_service_create(set);
    //input_key_service_add_key(input_ser, input_key_info, INPUT_KEY_NUM);
    //periph_service_set_callback(input_ser, input_key_service_cb, (void *)board_handle);

    ESP_LOGI(TAG, "[4.0] Create audio pipeline for playback");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    audio_player_t *player = calloc(1, sizeof(audio_player_t));
    AUDIO_MEM_CHECK(TAG, player, return NULL);
    player->pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(player->pipeline);

    ESP_LOGI(TAG, "[4.1] Create i2s stream to write data to codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.i2s_config.sample_rate = 48000;
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    player->i2s_stream_writer = i2s_stream_init(&i2s_cfg);
    AUDIO_MEM_CHECK(TAG, player->i2s_stream_writer, return NULL);

    ESP_LOGI(TAG, "[4.2] Create mp3 decoder to decode mp3 file");
    mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
    player->mp3_decoder = mp3_decoder_init(&mp3_cfg);
    AUDIO_MEM_CHECK(TAG, player->mp3_decoder, return NULL);
    audio_element_set_read_cb(player->mp3_decoder, my_sdcard_read_cb, NULL);

    /* ZL38063 audio chip on board of ESP32-LyraTD-MSC does not support 44.1 kHz sampling frequency,
       so resample filter has been added to convert audio data to other rates accepted by the chip.
       You can resample the data to 16 kHz or 48 kHz.
    */
    ESP_LOGI(TAG, "[4.3] Create resample filter");
    rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    player->rsp_handle = rsp_filter_init(&rsp_cfg);

    ESP_LOGI(TAG, "[4.4] Register all elements to audio pipeline");
    audio_pipeline_register(player->pipeline, player->mp3_decoder, "mp3");
    ESP_LOGI(TAG, "[4.4.2] Register all elements to audio pipeline");
    audio_pipeline_register(player->pipeline, player->i2s_stream_writer, "i2s");
    ESP_LOGI(TAG, "[4.4.3] Register all elements to audio pipeline");
    audio_pipeline_register(player->pipeline, player->rsp_handle, "filter");

    ESP_LOGI(TAG, "[4.5] Link it together [my_sdcard_read_cb]-->mp3_decoder-->i2s_stream-->[codec_chip]");
    audio_pipeline_link(player->pipeline, (const char *[]) {"mp3", "filter", "i2s"}, 3);
    return player;
//exit_tts_init:
    //google_tts_destroy(tts);
    //return NULL;
}

esp_err_t audio_player_destroy(audio_player_handle_t player)
{
    if (player == NULL) {
        return ESP_FAIL;
    }
    audio_pipeline_terminate(player->pipeline);
    audio_pipeline_remove_listener(player->pipeline);
    audio_pipeline_deinit(player->pipeline);
    audio_element_deinit(player->i2s_stream_writer);
    audio_element_deinit(player->mp3_decoder);
    free(player);
    return ESP_OK;
}

esp_err_t audio_player_set_listener(audio_player_handle_t player, audio_event_iface_handle_t listener)
{
    if (listener) {
        audio_pipeline_set_listener(player->pipeline, listener);
    }
    return ESP_OK;
}
/*
bool google_tts_check_event_finish(audio_player_handle_t player, audio_event_iface_msg_t *msg)
{
    if (msg->source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg->source == (void *) player->i2s_stream_writer
            && msg->cmd == AEL_MSG_CMD_REPORT_STATUS && (int) msg->data == AEL_STATUS_STATE_STOPPED) {
        return true;
    }
    return false;
}
*/
esp_err_t audio_player_start(audio_player_handle_t player)
{
    /*
    ESP_LOGI(TAG, "[5.0] Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[5.1] Listen for all pipeline events");
    audio_pipeline_set_listener(player->pipeline, evt);

    ESP_LOGW(TAG, "[ 6 ] Press the keys to control music player:");
    ESP_LOGW(TAG, "      [Play] to start, pause and resume, [Set] next song.");
    ESP_LOGW(TAG, "      [Vol-] or [Vol+] to adjust volume.");
    */
	audio_pipeline_run(player->pipeline);
    return ESP_OK;
/*
    while (1) {	
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
            continue;
        }
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT) {
            // Set music info for a new song to be played
            if (msg.source == (void *) mp3_decoder
                && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
                audio_element_info_t music_info = {0};
                audio_element_getinfo(mp3_decoder, &music_info);
                ESP_LOGI(TAG, "[ * ] Received music info from mp3 decoder, sample_rates=%d, bits=%d, ch=%d",
                         music_info.sample_rates, music_info.bits, music_info.channels);
                audio_element_setinfo(i2s_stream_writer, &music_info);
                rsp_filter_set_src_info(rsp_handle, music_info.sample_rates, music_info.channels);
                continue;
            }
            // Advance to the next song when previous finishes
            if (msg.source == (void *) i2s_stream_writer
                && msg.cmd == AEL_MSG_CMD_REPORT_STATUS) {
                audio_element_state_t el_state = audio_element_get_state(i2s_stream_writer);
                if (el_state == AEL_STATE_FINISHED) {
                    ESP_LOGI(TAG, "[ * ] Finished, advancing to the next song");
                    audio_pipeline_stop(pipeline);
                    audio_pipeline_wait_for_stop(pipeline);
                    //get_file(NEXT);
                    //audio_pipeline_run(pipeline);
                }
                continue;
            }
        }
    }
*/    
}

void audio_player_stop(audio_player_handle_t player)
{
    audio_pipeline_stop(player->pipeline);
    audio_pipeline_wait_for_stop(player->pipeline);
}