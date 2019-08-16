// Copyright 2015-2018 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/* C includes */
#include <stdio.h>

/* FreeRTOS includes */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

/* LVGL includes */
#include "iot_lvgl.h"

/* ESP32 includes */
#include "esp_log.h"

/**********************
 *      MACROS
 **********************/
#define CONTROL_CURRENT -1
#define CONTROL_NEXT -2
#define CONTROL_PREV -3
#define MAX_PLAY_FILE_NUM 20

#define USE_ADF_TO_PLAY CONFIG_USE_ADF_PLAY

//#if USE_ADF_TO_PLAY
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
//#endif

/**********************
 *      MACROS
 **********************/
#define TAG "coffee_lvgl"
#define LV_ANIM_RESOLUTION 1024
#define LV_ANIM_RES_SHIFT 10

// more files may be added and `MP3_FILE_COUNT` will reflect the actual count
#define MP3_FILE_COUNT sizeof(mp3_file)/sizeof(char*)

#define CURRENT 0
#define NEXT    1
/**********************
 *  STATIC VARIABLES
 **********************/
/* TabView Object */
static int8_t tab_id = 0;
static lv_obj_t *tabview = NULL;
static lv_obj_t *tab[3] = {NULL};

/* Button Object */
static lv_obj_t *prebtn[3] = {NULL};
static lv_obj_t *nextbtn[3] = {NULL};
static lv_obj_t *playbtn[3] = {NULL};

/* Label and slider Object */
static lv_obj_t *coffee_label[3] = {NULL};
static lv_obj_t *sweet_slider[3] = {NULL};
static lv_obj_t *weak_sweet[3] = {NULL};
static lv_obj_t *strong_sweet[3] = {NULL};

static lv_obj_t *play_arc[3] = {NULL};
static lv_obj_t *precent_label[3] = {NULL};

/**********************
 *  IMAGE DECLARE
 **********************/
LV_IMG_DECLARE(coffee_bean);
//LV_IMG_DECLARE(coffee_cup);
//LV_IMG_DECLARE(coffee_flower);

/* Image and txt resource */
const void *btn_img[] = {SYMBOL_PREV, SYMBOL_PLAY, SYMBOL_NEXT, SYMBOL_PAUSE};
//const void *wp_img[] = {&coffee_bean, &coffee_cup, &coffee_flower};
const void *wp_img[] = {&coffee_bean};
const char *coffee_type[] = {"RISTRETTO", "ESPRESSO", "AMERICANO"};

//#if USE_ADF_TO_PLAY
static audio_pipeline_handle_t pipeline;
static audio_element_handle_t fatfs_stream_reader, i2s_stream_writer, mp3_decoder;
//#endif


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

static lv_res_t audio_control(lv_obj_t *obj)
{
#if USE_ADF_TO_PLAY
    audio_element_state_t el_state = audio_element_get_state(i2s_stream_writer);
    switch (el_state) {
    case AEL_STATE_INIT:
        //lv_img_set_src(img[1], img_src[3]);
        audio_pipeline_run(pipeline);
        break;
    case AEL_STATE_RUNNING:
        //lv_img_set_src(img[1], img_src[1]);
        audio_pipeline_pause(pipeline);
        break;
    case AEL_STATE_PAUSED:
        //lv_img_set_src(img[1], img_src[3]);
        audio_pipeline_resume(pipeline);
        break;
    default:
        break;
    }
#else
    //play ? lv_img_set_src(img[1], img_src[1]) : lv_img_set_src(img[1], img_src[3]);
    //play = !play;
#endif
    return LV_RES_OK;
}

static lv_res_t prebtn_action(lv_obj_t *btn)
{
    if (--tab_id < 0) {
        tab_id = 2;
    }
    lv_tabview_set_tab_act(tabview, tab_id, true);
    return LV_RES_OK;
}

static lv_res_t nextbtn_action(lv_obj_t *btn)
{
    if (++tab_id > 2) {
        tab_id = 0;
    }
    lv_tabview_set_tab_act(tabview, tab_id, true);
    return LV_RES_OK;
}

static void play_callback(lv_obj_t *obj)
{
    lv_obj_set_pos(prebtn[tab_id], 15, 30);
    lv_obj_set_pos(nextbtn[tab_id], LV_HOR_RES - 50 - 15, 30);
    lv_obj_align(coffee_label[tab_id], tab[tab_id], LV_ALIGN_IN_TOP_MID, 0, 50);
    lv_obj_align(sweet_slider[tab_id], tab[tab_id], LV_ALIGN_IN_TOP_MID, -10, 100);
    lv_obj_align(weak_sweet[tab_id], sweet_slider[tab_id], LV_ALIGN_OUT_LEFT_MID, 0, 0);
    lv_obj_align(strong_sweet[tab_id], sweet_slider[tab_id], LV_ALIGN_OUT_RIGHT_MID, 0, 0);
    lv_obj_align(playbtn[tab_id], tab[tab_id], LV_ALIGN_IN_TOP_MID, 0, 170);

    lv_obj_animate(prebtn[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_IN, 400, 0, NULL);
    lv_obj_animate(nextbtn[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_IN, 400, 0, NULL);
    lv_obj_animate(coffee_label[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_IN, 400, 0, NULL);
    lv_obj_animate(sweet_slider[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_IN, 400, 0, NULL);
    lv_obj_animate(weak_sweet[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_IN, 400, 0, NULL);
    lv_obj_animate(strong_sweet[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_IN, 400, 0, NULL);
    lv_obj_animate(playbtn[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_IN, 400, 0, NULL);

    lv_obj_animate(play_arc[tab_id], LV_ANIM_FLOAT_BOTTOM | LV_ANIM_OUT, 400, 0, NULL);
    lv_obj_animate(precent_label[tab_id], LV_ANIM_FLOAT_BOTTOM | LV_ANIM_OUT, 400, 0, NULL);

    lv_tabview_set_sliding(tabview, true); /* must not sliding when play animation */
}

static void lv_play_arc_set_endangle(lv_obj_t *obj, lv_coord_t x)
{
    lv_arc_set_angles(obj, 0, x);
}

static int32_t play_arc_anim_path_linear(const lv_anim_t *a)
{
    /* Calculate the current step */

    uint16_t step;
    if (a->time == a->act_time) {
        step = LV_ANIM_RESOLUTION; /* Use the last value if the time fully elapsed */
    } else {
        step = (a->act_time * LV_ANIM_RESOLUTION) / a->time;
    }

    /* Get the new value which will be proportional to `step`
     * and the `start` and `end` values*/
    int32_t new_value;
    new_value = (int32_t)step * (a->end - a->start);
    new_value = new_value >> LV_ANIM_RES_SHIFT;
    new_value += a->start;

    char precent[5];
    sprintf(precent, "%3d%%", (int)(new_value * 100.0 / 360));
    lv_label_set_text(precent_label[tab_id], precent);

    return new_value;
}

static void play_arc_callback(lv_obj_t *play_arc)
{
    /* Create an animation to modify progress */
    lv_anim_t a;
    a.var = play_arc;
    a.start = 0;
    a.end = 360;
    a.fp = (lv_anim_fp_t)lv_play_arc_set_endangle;
    a.path = play_arc_anim_path_linear;
    a.end_cb = (lv_anim_cb_t)play_callback;
    a.act_time = 0;       /* Negative number to set a delay */
    a.time = 3000;        /* Animate in 3000 ms */
    a.playback = 0;       /* Make the animation backward too when it's ready */
    a.playback_pause = 0; /* Wait before playback */
    a.repeat = 0;         /* Repeat the animation */
    a.repeat_pause = 0;   /* Wait before repeat */
    lv_anim_create(&a);
}
static lv_res_t playbtn_action(lv_obj_t *btn)
{
    lv_obj_animate(prebtn[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_OUT, 400, 0, NULL);
    lv_obj_animate(nextbtn[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_OUT, 400, 0, NULL);
    lv_obj_animate(coffee_label[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_OUT, 400, 0, NULL);
    lv_obj_animate(sweet_slider[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_OUT, 400, 0, NULL);
    lv_obj_animate(weak_sweet[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_OUT, 400, 0, NULL);
    lv_obj_animate(strong_sweet[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_OUT, 400, 0, NULL);
    lv_obj_animate(playbtn[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_OUT, 400, 0, NULL);

    lv_arc_set_angles(play_arc[tab_id], 0, 0);
    lv_label_set_text(precent_label[tab_id], "  0%");
    lv_obj_align(play_arc[tab_id], tab[tab_id], LV_ALIGN_IN_TOP_MID, 0, (LV_VER_RES - lv_obj_get_height(play_arc[tab_id])) / 2 + 10);
    lv_obj_align(precent_label[tab_id], tab[tab_id], LV_ALIGN_IN_TOP_MID, 0, (LV_VER_RES - lv_obj_get_height(play_arc[tab_id])) / 2 + 80);
    lv_obj_animate(play_arc[tab_id], LV_ANIM_FLOAT_BOTTOM | LV_ANIM_IN, 400, 0, play_arc_callback);
    lv_obj_animate(precent_label[tab_id], LV_ANIM_FLOAT_BOTTOM | LV_ANIM_IN, 400, 0, NULL);

    lv_tabview_set_sliding(tabview, false); /* must not sliding when play animation */

    return LV_RES_OK;
}

static void play_arc_first_callback(lv_obj_t *play_arc)
{
    lv_obj_set_hidden(play_arc, false);
}

static void create_tab(lv_obj_t *parent, uint8_t wp_img_id, uint8_t coffee_type_id, uint8_t id)
{
    lv_page_set_sb_mode(parent, LV_SB_MODE_OFF);
    lv_obj_set_protect(parent, LV_PROTECT_PARENT | LV_PROTECT_POS | LV_PROTECT_FOLLOW);
    lv_page_set_scrl_fit(parent, false, false);       /* It must not be automatically sized to allow all children to participate. */
    lv_page_set_scrl_height(parent, LV_VER_RES + 20); /* Set height of the scrollable part of a page */

    lv_obj_t *wp = lv_img_create(parent, NULL); /* create wallpaper */
    lv_img_set_src(wp, wp_img[wp_img_id]);      /* set wallpaper image */

    static lv_style_t btn_rel_style;
    static lv_style_t btn_pr_style;

    prebtn[id] = lv_btn_create(parent, NULL); /* Create previous page btn */
    lv_obj_set_size(prebtn[id], 50, 50);
    lv_obj_t *preimg = lv_img_create(prebtn[id], NULL);
    lv_img_set_src(preimg, btn_img[0]);
    lv_obj_set_pos(prebtn[id], 15, 30);
    lv_btn_set_action(prebtn[id], LV_BTN_ACTION_CLICK, audio_control);
    lv_style_copy(&btn_rel_style, lv_btn_get_style(prebtn[id], LV_BTN_STYLE_REL));
    lv_style_copy(&btn_pr_style, lv_btn_get_style(prebtn[id], LV_BTN_STYLE_PR));
    btn_rel_style.body.main_color = LV_COLOR_WHITE;
    btn_pr_style.body.main_color = LV_COLOR_WHITE;
    btn_rel_style.body.grad_color = LV_COLOR_WHITE;
    btn_pr_style.body.grad_color = LV_COLOR_WHITE;
    btn_rel_style.body.border.color = LV_COLOR_WHITE;
    btn_pr_style.body.border.color = LV_COLOR_WHITE;
    btn_rel_style.body.shadow.color = LV_COLOR_WHITE;
    btn_pr_style.body.shadow.color = LV_COLOR_WHITE;
    btn_rel_style.text.color = LV_COLOR_WHITE;
    btn_pr_style.text.color = LV_COLOR_WHITE;
    btn_rel_style.image.color = LV_COLOR_WHITE;
    btn_pr_style.image.color = LV_COLOR_WHITE;
    btn_rel_style.line.color = LV_COLOR_WHITE;
    btn_pr_style.line.color = LV_COLOR_WHITE;

    lv_img_set_style(preimg, &btn_rel_style);
    lv_btn_set_style(prebtn[id], LV_BTN_STYLE_REL, &btn_rel_style);
    lv_btn_set_style(prebtn[id], LV_BTN_STYLE_PR, &btn_pr_style);

    nextbtn[id] = lv_btn_create(parent, NULL); /* Create next page btn */
    lv_obj_set_size(nextbtn[id], 50, 50);
    lv_obj_t *nextimg = lv_img_create(nextbtn[id], NULL);
    lv_img_set_src(nextimg, btn_img[2]);
    lv_obj_set_pos(nextbtn[id], LV_HOR_RES - 50 - 15, 30);
    lv_btn_set_action(nextbtn[id], LV_BTN_ACTION_CLICK, nextbtn_action);

    lv_img_set_style(nextimg, &btn_rel_style);
    lv_btn_set_style(nextbtn[id], LV_BTN_STYLE_REL, &btn_rel_style);
    lv_btn_set_style(nextbtn[id], LV_BTN_STYLE_PR, &btn_pr_style);

    /* Create a new style for the label */
    static lv_style_t coffee_style;
    coffee_label[id] = lv_label_create(parent, NULL); /* Create coffee type label */
    lv_label_set_text(coffee_label[id], coffee_type[coffee_type_id]);
    lv_style_copy(&coffee_style, lv_label_get_style(coffee_label[id]));
    coffee_style.text.color = LV_COLOR_WHITE;
    coffee_style.text.font = &lv_font_dejavu_30; /* Unicode and symbol fonts already assigned by the library */
    lv_label_set_style(coffee_label[id], &coffee_style);
    lv_obj_align(coffee_label[id], parent, LV_ALIGN_IN_TOP_MID, 0, 50); /* Align to parent */

    /* Create a bar, an indicator and a knob style */
    static lv_style_t style_bar;
    static lv_style_t style_indic;
    static lv_style_t style_knob;

    lv_style_copy(&style_bar, &lv_style_pretty);
    style_bar.body.main_color = LV_COLOR_BLACK;
    style_bar.body.grad_color = LV_COLOR_GRAY;
    style_bar.body.radius = LV_RADIUS_CIRCLE;
    style_bar.body.border.color = LV_COLOR_WHITE;
    style_bar.body.opa = LV_OPA_60;
    style_bar.body.padding.hor = 0;
    style_bar.body.padding.ver = LV_DPI / 10;

    lv_style_copy(&style_indic, &lv_style_pretty);
    style_indic.body.grad_color = LV_COLOR_WHITE;
    style_indic.body.main_color = LV_COLOR_WHITE;
    style_indic.body.radius = LV_RADIUS_CIRCLE;
    style_indic.body.shadow.width = LV_DPI / 10;
    style_indic.body.shadow.color = LV_COLOR_WHITE;
    style_indic.body.padding.hor = LV_DPI / 30;
    style_indic.body.padding.ver = LV_DPI / 30;

    lv_style_copy(&style_knob, &lv_style_pretty);
    style_knob.body.radius = LV_RADIUS_CIRCLE;
    style_knob.body.opa = LV_OPA_70;

    sweet_slider[id] = lv_slider_create(parent, NULL); /* Create a sweetness adjustment slider */
    lv_slider_set_style(sweet_slider[id], LV_SLIDER_STYLE_BG, &style_bar);
    lv_slider_set_style(sweet_slider[id], LV_SLIDER_STYLE_INDIC, &style_indic);
    lv_slider_set_style(sweet_slider[id], LV_SLIDER_STYLE_KNOB, &style_knob);
    lv_obj_set_size(sweet_slider[id], LV_HOR_RES - 140, 40);               /* set object size */
    lv_obj_align(sweet_slider[id], parent, LV_ALIGN_IN_TOP_MID, -10, 100); /* Align to parent */
    lv_slider_set_range(sweet_slider[id], 0, 100);                         /* set slider range */
    lv_slider_set_value(sweet_slider[id], 50);                             /* set slider current value */

    static lv_style_t sweet_style;

    weak_sweet[id] = lv_label_create(parent, NULL); /* Create weak sweet label */
    lv_label_set_text(weak_sweet[id], "WEAK");
    lv_style_copy(&sweet_style, lv_label_get_style(weak_sweet[id]));
    sweet_style.text.color = LV_COLOR_WHITE;
    sweet_style.text.font = &lv_font_dejavu_20; /* Unicode and symbol fonts already assigned by the library */
    lv_label_set_style(weak_sweet[id], &sweet_style);
    lv_obj_align(weak_sweet[id], sweet_slider[id], LV_ALIGN_OUT_LEFT_MID, 0, 0); /* Align to sweet_slider */

    strong_sweet[id] = lv_label_create(parent, NULL); /* Create strong sweet label */
    lv_label_set_text(strong_sweet[id], "STRONG");
    lv_label_set_style(strong_sweet[id], &sweet_style);
    lv_obj_align(strong_sweet[id], sweet_slider[id], LV_ALIGN_OUT_RIGHT_MID, 0, 0); /* Align to sweet_slider */

    playbtn[id] = lv_btn_create(parent, NULL); /* Create a make button */
    lv_obj_set_size(playbtn[id], 70, 70);      /* set object size */
    lv_obj_t *playimg = lv_img_create(playbtn[id], NULL);
    lv_img_set_src(playimg, btn_img[1]);
    lv_obj_align(playbtn[id], parent, LV_ALIGN_IN_TOP_MID, 0, 170); /* Align to parent */
    lv_btn_set_action(playbtn[id], LV_BTN_ACTION_CLICK, playbtn_action);

    lv_img_set_style(playimg, &btn_rel_style);
    lv_btn_set_style(playbtn[id], LV_BTN_STYLE_REL, &btn_rel_style);
    lv_btn_set_style(playbtn[id], LV_BTN_STYLE_PR, &btn_pr_style);
    lv_obj_set_protect(playbtn[id], LV_PROTECT_PARENT | LV_PROTECT_POS | LV_PROTECT_FOLLOW);

    lv_obj_animate(prebtn[id], LV_ANIM_FLOAT_TOP | LV_ANIM_IN, 400, 0, NULL);
    lv_obj_animate(nextbtn[id], LV_ANIM_FLOAT_TOP | LV_ANIM_IN, 400, 0, NULL);
    lv_obj_animate(coffee_label[id], LV_ANIM_FLOAT_TOP | LV_ANIM_IN, 400, 0, NULL);
    lv_obj_animate(sweet_slider[id], LV_ANIM_FLOAT_TOP | LV_ANIM_IN, 400, 0, NULL);
    lv_obj_animate(weak_sweet[id], LV_ANIM_FLOAT_TOP | LV_ANIM_IN, 400, 0, NULL);
    lv_obj_animate(strong_sweet[id], LV_ANIM_FLOAT_TOP | LV_ANIM_IN, 400, 0, NULL);
    lv_obj_animate(playbtn[id], LV_ANIM_FLOAT_BOTTOM | LV_ANIM_IN, 400, 0, NULL);

    play_arc[id] = lv_arc_create(parent, NULL); /* Create progress disk */
    lv_obj_set_size(play_arc[id], 180, 180);    /* set object size */
    lv_arc_set_angles(play_arc[id], 0, 0);      /* set current angle */

    static lv_style_t arc_style;
    lv_style_copy(&arc_style, lv_arc_get_style(play_arc[id], LV_ARC_STYLE_MAIN));
    arc_style.line.color = LV_COLOR_WHITE;
    arc_style.line.width = 10;
    lv_arc_set_style(play_arc[id], LV_ARC_STYLE_MAIN, &arc_style);
    lv_obj_set_hidden(play_arc[id], true);                                                                               /* hide the object */
    lv_obj_align(play_arc[id], parent, LV_ALIGN_IN_TOP_MID, 0, (LV_VER_RES - lv_obj_get_height(play_arc[id])) / 2 + 10); /* Align to parent */
    lv_obj_animate(play_arc[id], LV_ANIM_FLOAT_BOTTOM | LV_ANIM_OUT, 400, 0, play_arc_first_callback);                   /* animation of hide the object */

    static lv_style_t precent_label_style;
    precent_label[id] = lv_label_create(parent, NULL); /* Create progress label */
    lv_label_set_text(precent_label[id], "  0%");
    lv_style_copy(&precent_label_style, lv_label_get_style(precent_label[id]));
    precent_label_style.text.color = LV_COLOR_WHITE;
    precent_label_style.text.font = &lv_font_dejavu_40; /* Unicode and symbol fonts already assigned by the library */
    lv_label_set_style(precent_label[id], &precent_label_style);
    lv_obj_align(precent_label[id], parent, LV_ALIGN_IN_TOP_MID, 0, (LV_VER_RES - lv_obj_get_height(precent_label[id])) / 2 + 80); /* Align to parent */
    lv_obj_animate(precent_label[id], LV_ANIM_FLOAT_BOTTOM | LV_ANIM_OUT, 400, 0, NULL);                                           /* animation of hide the object */
}

static lv_res_t tabview_load_action(lv_obj_t *tabview, uint16_t tab_action_id)
{
    tab_id = tab_action_id;
    return LV_RES_OK;
}

static void littlevgl_coffee(void)
{
    lv_theme_set_current(lv_theme_zen_init(100, NULL));

    tabview = lv_tabview_create(lv_scr_act(), NULL);
    lv_obj_set_size(tabview, LV_HOR_RES + 16, LV_VER_RES + 90);
    lv_obj_set_pos(tabview, -8, -60);

    tab[0] = lv_tabview_add_tab(tabview, "RIS"); /* add RIS tab */
    tab[1] = lv_tabview_add_tab(tabview, "ESP"); /* add ESP tab */
    tab[2] = lv_tabview_add_tab(tabview, "AME"); /* add AME tab */
    lv_tabview_set_tab_load_action(tabview, tabview_load_action);
    lv_obj_set_protect(tabview, LV_PROTECT_PARENT | LV_PROTECT_POS | LV_PROTECT_FOLLOW);
    lv_tabview_set_anim_time(tabview, 0);

    create_tab(tab[0], 0, 0, 0); /* Create ristretto coffee selection tab page */
    //create_tab(tab[1], 1, 1, 1); /* Create espresso coffee selection tab page */
    //create_tab(tab[2], 2, 2, 2); /* Create americano coffee selection tab page */
}

static void user_task(void *pvParameter)
{
    while (1) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

//#if USE_ADF_TO_PLAY
/*
* Callback function to feed audio data stream from sdcard to mp3 decoder element
*/
static int my_sdcard_read_cb(audio_element_handle_t el, char *buf, int len, TickType_t wait_time, void *ctx)
{
    int read_len = fread(buf, 1, len, get_file(CONTROL_CURRENT));
    if (read_len == 0) {
        read_len = AEL_IO_DONE;
    }
    return read_len;
}

static void audio_sdcard_task(void *para)
{
    ESP_LOGI(TAG, "[ 1 ] Mount sdcard");
    // Initialize peripherals management
    esp_periph_config_t periph_cfg = {\
    .task_stack         = 4096,   \
    .task_prio          = 1,    \
    .task_core          = 0,    \
    };
    //esp_periph_config_t periph_cfg = {0};
    //esp_periph_init(&periph_cfg);
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);
// Initialize SD Card peripheral
    audio_board_sdcard_init(set);

    ESP_LOGI(TAG, "[ 2 ] Start codec chip");
    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);

    ESP_LOGI(TAG, "[3.0] Create audio pipeline for playback");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline);

    ESP_LOGI(TAG, "[3.1] Create fatfs stream to read data from sdcard");
    fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_cfg.type = AUDIO_STREAM_READER;
    fatfs_stream_reader = fatfs_stream_init(&fatfs_cfg);

    ESP_LOGI(TAG, "[3.2] Create i2s stream to write data to codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_stream_writer = i2s_stream_init(&i2s_cfg);

    ESP_LOGI(TAG, "[3.3] Create mp3 decoder to decode mp3 file");
    mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
    mp3_decoder = mp3_decoder_init(&mp3_cfg);

    ESP_LOGI(TAG, "[3.4] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline, fatfs_stream_reader, "file");
    audio_pipeline_register(pipeline, mp3_decoder, "mp3");
    audio_pipeline_register(pipeline, i2s_stream_writer, "i2s");

    ESP_LOGI(TAG, "[3.5] Link it together [sdcard]-->fatfs_stream-->mp3_decoder-->i2s_stream-->[codec_chip]");
    audio_pipeline_link(pipeline, (const char *[]) {"file", "mp3", "i2s"}, 3);

    ESP_LOGI(TAG, "[3.6] Set up  uri (file as fatfs_stream, mp3 as mp3 decoder, and default output is i2s)");
    audio_element_set_uri(fatfs_stream_reader, "/sdcard/winxp.mp3");


    ESP_LOGI(TAG, "[ 4 ] Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[4.1] Listening event from all elements of pipeline");
    audio_pipeline_set_listener(pipeline, evt);

    ESP_LOGI(TAG, "[4.2] Listening event from peripherals");
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);


    ESP_LOGI(TAG, "[ 5 ] Start audio_pipeline");
    //audio_pipeline_run(pipeline);
    ESP_LOGI(TAG, "[APP] Free memory*****: %d bytes", esp_get_free_heap_size());
/*
    //audio_board_sdcard_init(set);

    // Initialize SD Card peripheral
    periph_sdcard_cfg_t sdcard_cfg = {
        .root = "/sdcard",
        .card_detect_pin = 39, //GPIO_NUM_34
    };
    esp_periph_handle_t sdcard_handle = periph_sdcard_init(&sdcard_cfg);
    // Start sdcard & button peripheral
    esp_periph_start(set, sdcard_handle);

    // Wait until sdcard was mounted
    while (!periph_sdcard_is_mounted(sdcard_handle)) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    strcpy(current_mp3_name, "/sdcard/start.mp3");

    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);
    ESP_LOGI(TAG, "[2.0] Create audio pipeline for playback");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline);

    ESP_LOGI(TAG, "[2.1] Create i2s stream to write data to ESP32 internal DAC");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_cfg.i2s_config.sample_rate = 48000;
    i2s_stream_writer = i2s_stream_init(&i2s_cfg);

    ESP_LOGI(TAG, "[2.2] Create mp3 decoder to decode mp3 file");
    mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
    mp3_decoder = mp3_decoder_init(&mp3_cfg);
    audio_element_set_read_cb(mp3_decoder, my_sdcard_read_cb, NULL);

    ESP_LOGI(TAG, "[2.3] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline, mp3_decoder, "mp3");
    audio_pipeline_register(pipeline, i2s_stream_writer, "i2s");

    ESP_LOGI(TAG, "[2.4] Link it together [my_sdcard_read_cb]-->mp3_decoder-->i2s_stream-->[codec_chip]");
    audio_pipeline_link(pipeline, (const char *[]) {
        "mp3", "i2s"
    }, 2);

    ESP_LOGI(TAG, "[ 3 ] Setup event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[3.1] Listening event from all elements of pipeline");
    audio_pipeline_set_listener(pipeline, evt);

    ESP_LOGI(TAG, "[3.2] Listening event from peripherals");
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);
    //audio_event_iface_set_listener(esp_periph_get_event_iface(), evt);

    ESP_LOGI(TAG, "[ 4 ] Listen for all pipeline events");

    audio_pipeline_run(pipeline);
    */
    while (1) {
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
            continue;
        }

        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *)mp3_decoder && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
            audio_element_info_t music_info = {0};
            audio_element_getinfo(mp3_decoder, &music_info);

            ESP_LOGI(TAG, "[ * ] Receive music info from mp3 decoder, sample_rates=%d, bits=%d, ch=%d",
                     music_info.sample_rates, music_info.bits, music_info.channels);

            audio_element_setinfo(i2s_stream_writer, &music_info);
            i2s_stream_set_clk(i2s_stream_writer, music_info.sample_rates, music_info.bits, music_info.channels);
            continue;
        }

        // Advance to the next song when previous finishes
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *)i2s_stream_writer && msg.cmd == AEL_MSG_CMD_REPORT_STATUS) {
            audio_element_state_t el_state = audio_element_get_state(i2s_stream_writer);
            if (el_state == AEL_STATE_FINISHED) {
                ESP_LOGI(TAG, "[ * ] Finished, advancing to the next song");
                audio_pipeline_stop(pipeline);
                audio_pipeline_wait_for_stop(pipeline);
                //get_file(CONTROL_NEXT);
                //audio_pipeline_run(pipeline);
            }
            continue;
        }

        /* Stop when the last pipeline element (i2s_stream_writer in this case) receives stop event */
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *)i2s_stream_writer && msg.cmd == AEL_MSG_CMD_REPORT_STATUS && (int)msg.data == AEL_STATUS_STATE_STOPPED) {
            ESP_LOGW(TAG, "[ * ] Stop event received");
            break;
        }
    }

    ESP_LOGI(TAG, "[ 7 ] Stop audio_pipeline");
    audio_pipeline_terminate(pipeline);

    /* Terminal the pipeline before removing the listener */
    audio_pipeline_remove_listener(pipeline);

    /* Stop all periph before removing the listener */
    //esp_periph_stop_all();
    //audio_event_iface_remove_listener(esp_periph_get_event_iface(), evt);

    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    audio_event_iface_destroy(evt);

    /* Release all resources */
    audio_pipeline_deinit(pipeline);
    audio_element_deinit(i2s_stream_writer);
    audio_element_deinit(mp3_decoder);
    //esp_periph_destroy();
    vTaskDelete(NULL);
}
//#endif

/******************************************************************************
 * FunctionName : app_main
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void app_main()
{
    /* Initialize LittlevGL GUI */
    lvgl_init();

    /* coffee demo */
    littlevgl_coffee();

    xTaskCreate(
        user_task,   // Task Function
        "user_task", // Task Name
        1024,        // Stack Depth
        NULL,        // Parameters
        1,           // Priority
        NULL);       // Task Handler

//#if USE_ADF_TO_PLAY
    gpio_set_pull_mode((gpio_num_t)15, GPIO_PULLUP_ONLY); // CMD, needed in 4- and 1- line modes
    gpio_set_pull_mode((gpio_num_t)2, GPIO_PULLUP_ONLY);  // D0, needed in 4- and 1- line modes
    gpio_set_pull_mode((gpio_num_t)4, GPIO_PULLUP_ONLY);  // D1, needed in 4-line mode only
    gpio_set_pull_mode((gpio_num_t)12, GPIO_PULLUP_ONLY); // D2, needed in 4-line mode only
    //gpio_set_pull_mode((gpio_num_t)13, GPIO_PULLUP_ONLY); // D3, needed in 4- and 1- line modes
    xTaskCreate(audio_sdcard_task, "audio_sdcard_task", 1024 * 10, NULL, 0, NULL);
//#endif

    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
}
