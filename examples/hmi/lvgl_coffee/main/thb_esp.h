/* component includes */
#include <stdio.h>
#include <string.h>

/* freertos includes */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_freertos_hooks.h"

/* esp includes */
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_err.h"
#include "ff.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"
#include "cJSON.h"

/* Param Include */
#include "iot_param.h"
#include "nvs_flash.h"

/* Audio */
#include "audio_hal.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_mem.h"
#include "audio_common.h"
#include "i2s_stream.h"
#include "mp3_decoder.h"
#include "fatfs_stream.h"
#include "esp_peripherals.h"
#include "periph_sdcard.h"
#include "board.h"
#include "filter_resample.h"
#include "input_key_service.h"
#include "periph_adc_button.h"

/* Hardware */
#include "driver/uart.h"
#include "iot_lcd.h"

#include "audio.h"
//#include "menu.h"
//#include "st7735_lcd.h"
//#include "draw.h"
#include "baidu_sr.h"

#include "esp_event_loop.h"


