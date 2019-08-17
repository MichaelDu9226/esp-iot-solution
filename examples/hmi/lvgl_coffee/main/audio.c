#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "esp_http_client.h"
#include "sdkconfig.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "board.h"
#include "esp_peripherals.h"
#include "periph_button.h"
#include "periph_wifi.h"
#include "periph_led.h"
#include "baidu_sr.h"
#include "audio_player.h"
#include "board.h"

#include "audio_element.h"
#include "filter_resample.h"
#include "mp3_decoder.h"
#include "i2s_stream.h"

#define TAG "audio"

audio_player_handle_t player;
baidu_sr_handle_t sr;
uint8_t voice_control_state = 0;
uint8_t wait_voice_control_state = 0;

static void es8388_pa_power(bool enable)
{
    gpio_config_t  io_conf;
    memset(&io_conf, 0, sizeof(io_conf));
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = BIT(21);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    if (enable) {
        gpio_set_level(21, 1);
    } else {
        gpio_set_level(21, 0);
    }
}

void audio_task(void *pv)
{
    ESP_LOGI(TAG, "Free heap1: %u", xPortGetFreeHeapSize());
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    tcpip_adapter_init();
    ESP_LOGI(TAG, "Free heap2: %u", xPortGetFreeHeapSize());
    ////ESP_LOGI(TAG, "[ 1 ] Initialize Buttons & Connect to Wi-Fi network, ssid=%s", CONFIG_WIFI_SSID);
    // Initialize peripherals management
    esp_periph_config_t periph_cfg = {\
    .task_stack         = 4096,   \
    .task_prio          = 5,    \
    .task_core          = 1,    \
};
    //DEFAULT_ESP_PHERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    periph_wifi_cfg_t wifi_cfg = {
        .ssid = "TP-LINK_HEXBOT",//CONFIG_WIFI_SSID,
        .password = "HEXBOT01?02?03",//CONFIG_WIFI_PASSWORD,
        //.ssid = "803",//CONFIG_WIFI_SSID,
        //.password = "20120601",//CONFIG_WIFI_PASSWORD,
    };
    esp_periph_handle_t wifi_handle = periph_wifi_init(&wifi_cfg);
    ESP_LOGI(TAG, "Free heap3: %u", xPortGetFreeHeapSize());
    // Initialize Button peripheral
    periph_button_cfg_t btn_cfg = {
        .gpio_mask = (1ULL << get_input_mode_id()) | (1ULL << get_input_rec_id()),
    };
    esp_periph_handle_t button_handle = periph_button_init(&btn_cfg);

    // Start wifi & button peripheral
    esp_periph_start(set, button_handle);
    esp_periph_start(set, wifi_handle);

    periph_wifi_wait_for_connected(wifi_handle, portMAX_DELAY);

    //ESP_LOGI(TAG, "[ 2 ] Start codec chip");
    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);

    baidu_sr_config_t sr_config = {
        .api_key = "AIzaSyAA_GqSv9wJfCcGDC5Bdn9wJ6iNAZWrbaY",//CONFIG_GOOGLE_API_KEY,
        .lang_code = "en-US",//GOOGLE_SR_LANG,
        .record_sample_rates = 16000//EXAMPLE_RECORD_PLAYBACK_SAMPLE_RATE,
        //.encoding = 0,
        //.on_begin = google_sr_begin,
    };
    sr = baidu_sr_init(&sr_config);
    
    ESP_LOGI(TAG, "Free heap4: %u", xPortGetFreeHeapSize());
    audio_player_config_t player_config = {
        .api_key = "AIzaSyAA_GqSv9wJfCcGDC5Bdn9wJ6iNAZWrbaY",//CONFIG_GOOGLE_API_KEY,
        .lang_code = "en-US",//GOOGLE_SR_LANG,
        .record_sample_rates = 16000,//EXAMPLE_RECORD_PLAYBACK_SAMPLE_RATE,
    };
    player = audio_player_init(&player_config);
    
    ESP_LOGI(TAG, "Free heap5: %u", xPortGetFreeHeapSize());
    ESP_LOGI(TAG, "[ 4 ] Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[4.1] Listening event from the pipeline");
    baidu_sr_set_listener(sr, evt);
    //audio_player_set_listener(player, evt);

    ESP_LOGI(TAG, "[4.2] Listening event from peripherals");
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);

    audio_player_start(player);
    //es8388_pa_power(1);
    ESP_LOGI(TAG, "[ 5 ] Listen for all pipeline events");
    while (1) {
        audio_event_iface_msg_t msg;
        if (audio_event_iface_listen(evt, &msg, portMAX_DELAY) != ESP_OK) {
            //ESP_LOGW(TAG, "[ * ] Event process failed: src_type:%d, source:%p cmd:%d, data:%p, data_len:%d",
                     //msg.source_type, msg.source, msg.cmd, msg.data, msg.data_len);
            continue;
        }

        //////ESP_LOGI(TAG, "[ * ] Event received: src_type:%d, source:%p cmd:%d, data:%p, data_len:%d",
                 //msg.source_type, msg.source, msg.cmd, msg.data, msg.data_len);
/*
        if (google_tts_check_event_finish(tts, &msg)) {
            //ESP_LOGI(TAG, "[ * ] TTS Finish");
            continue;
        }
*/

        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT) {
            // Set music info for a new song to be played
            if (msg.source == (void *) player->mp3_decoder
                && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
                audio_element_info_t music_info = {0};
                audio_element_getinfo(player->mp3_decoder, &music_info);
                ESP_LOGI(TAG, "[ * ] Received music info from mp3 decoder, sample_rates=%d, bits=%d, ch=%d",
                         music_info.sample_rates, music_info.bits, music_info.channels);
                audio_element_setinfo(player->i2s_stream_writer, &music_info);
                i2s_stream_set_clk(player->i2s_stream_writer, music_info.sample_rates, music_info.bits, music_info.channels);
                //rsp_filter_set_src_info(player->rsp_handle, music_info.sample_rates, music_info.channels);
                //vTaskDelay(1000 / portTICK_RATE_MS);
                //es8388_pa_power(1);
                continue;
            }
            // Advance to the next song when previous finishes
            if (msg.source == (void *) player->i2s_stream_writer
                && msg.cmd == AEL_MSG_CMD_REPORT_STATUS) {
                audio_element_state_t el_state = audio_element_get_state(player->i2s_stream_writer);
                if (el_state == AEL_STATE_FINISHED) {
                    if(voice_control_state == 0){
                        audio_player_stop(player);
                    }else if(voice_control_state == 1){
                        audio_player_stop(player);
                        baidu_sr_start(sr);
                    }
                    ////ESP_LOGI(TAG, "[ * ] Finished, advancing to the next song");
                    //audio_player_stop(player);
                    //audio_player_pipeline_stop(player->pipeline);
                    //audio_player_pipeline_wait_for_stop(player->pipeline);
                    //get_file(NEXT);
                    //audio_pipeline_run(pipeline);
                }
                continue;
            }
        }

        if (msg.source_type != PERIPH_ID_BUTTON) {
            continue;
        }

        // It's MODE button
        if ((int)msg.data == get_input_mode_id()) {
            break;
        }

        if ((int)msg.data != get_input_rec_id()) {
            continue;
        }

        if (msg.cmd == PERIPH_BUTTON_PRESSED) {
            //google_tts_stop(tts);
            //ESP_LOGI(TAG, "[ * ] Resuming pipeline");
            audio_player_stop(player);
            baidu_sr_start(sr);
        } else if (msg.cmd == PERIPH_BUTTON_RELEASE || msg.cmd == PERIPH_BUTTON_LONG_RELEASE) {
            ESP_LOGI(TAG, "[ * ] Stop pipeline");

            //periph_led_stop(led_handle, get_green_led_gpio());
            ESP_LOGI(TAG, "baidu_sr sample_rates:%d", sr->sample_rates);
            char *original_text = baidu_sr_stop(sr);
            if (original_text == NULL) {
                ESP_LOGI(TAG, "original_text == NULL");
                continue;
            }
            ESP_LOGI(TAG, "Original text = %s", original_text);
            audio_player_play(player, "/sdcard/winxp.mp3");
            /*
            char *translated_text = google_translate(original_text, GOOGLE_TRANSLATE_LANG_FROM, GOOGLE_TRANSLATE_LANG_TO, CONFIG_GOOGLE_API_KEY);
            if (translated_text == NULL) {
                continue;
            }
            //ESP_LOGI(TAG, "Translated text = %s", translated_text);
            google_tts_start(tts, translated_text, GOOGLE_TTS_LANG);
            */
        }

    }
    //ESP_LOGI(TAG, "[ 6 ] Stop audio_pipeline");
    //baidu_sr_destroy(sr);
    //baidu_tts_destroy(tts);
    /* Stop all periph before removing the listener */
    esp_periph_set_stop_all(set);
    audio_event_iface_remove_listener(esp_periph_set_get_event_iface(set), evt);

    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    audio_event_iface_destroy(evt);
    esp_periph_set_destroy(set);
    vTaskDelete(NULL);
}

void play_mp3(char* mp3name)
{   
    voice_control_state = 0;
    audio_player_stop(player);
    audio_player_play(player, mp3name);
}

void play_mp3_stop()
{   
    voice_control_state = 0;
    audio_player_stop(player);
}
void voice_control_start(void)
{
    voice_control_state = 1;
    //audio_player_stop(player);
    audio_player_play(player, "/sdcard/ding.mp3");
    wait_voice_control_state = 1;
}

void voice_control_stop(void)
{
    char *original_text = baidu_sr_stop(sr);
    //if (original_text == NULL) {
        //ESP_LOGI(TAG, "original_text == NULL");
        //continue;
    //}
    //ESP_LOGI(TAG, "Original text = %s", original_text);
}

void audio_start(void)
{
    //esp_err_t err = nvs_flash_init();
    xTaskCreate(audio_task, "audio_task", 8 * 1024, NULL, 5, NULL);
}