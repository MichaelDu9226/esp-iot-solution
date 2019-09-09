// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
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
#include "iot_xpt2046.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define TAG "CXpt2046_obj"

CXpt2046::CXpt2046(xpt_conf_t * xpt_conf, int rotation)
{
    iot_xpt2046_init(xpt_conf, &m_spi);
    m_pressed = false;
    m_rotation = rotation;
    m_io_irq = xpt_conf->pin_num_irq;
    if (m_spi_mux == NULL) {
        m_spi_mux = xSemaphoreCreateRecursiveMutex();
    }
}

bool CXpt2046::is_pressed()
{
    //sample();
    get_sample(TOUCH_CMD_X);
    if(gpio_get_level((gpio_num_t) m_io_irq) == 0){
        //ESP_LOGI(TAG, "###");
        sample();
        m_pressed = true;
    } else {
        //ESP_LOGI(TAG, "***");
        m_pressed = false;
    }
    return m_pressed;
}

int CXpt2046::get_irq()
{
    return gpio_get_level((gpio_num_t) m_io_irq);
}

void CXpt2046::set_rotation(int rotation)
{
    m_rotation = rotation % 4;
}

position CXpt2046::get_raw_position()
{
    return m_pos;
}

int CXpt2046::get_sample(uint8_t command)
{
    xSemaphoreTakeRecursive(m_spi_mux, portMAX_DELAY);
    int data = iot_xpt2046_readdata(m_spi, command);
    xSemaphoreGiveRecursive(m_spi_mux);
    return data;
}

/*
void CXpt2046::sample()
{
    position samples[XPT2046_SMPSIZE];
    position distances[XPT2046_SMPSIZE];
    position samples_x[XPT2046_SMPSIZE];
    position samples_y[XPT2046_SMPSIZE];
    m_pressed = true;

    int aveX = 0;
    int aveY = 0;
    get_sample(TOUCH_CMD_X);
    get_sample(TOUCH_CMD_Y);
    get_sample(TOUCH_CMD_X);
    get_sample(TOUCH_CMD_Y);
    for (int i = 0; i < XPT2046_SMPSIZE; i++) {
        samples_x[i].x = i;
        samples_x[i].y = get_sample(TOUCH_CMD_X);
        samples_y[i].x = i;
        samples_y[i].y = get_sample(TOUCH_CMD_Y);

        if (samples[i].x == 0 || samples[i].x == XPT2046_SMP_MAX || samples[i].y == 0
                || samples[i].y == XPT2046_SMP_MAX) {
            m_pressed = false;
            //ESP_LOGI(TAG, "False");
        }
        //aveX += samples[i].x;
        //aveY += samples[i].y;
    }

    // sort by distance
    for (int i = 0; i < XPT2046_SMPSIZE - 1; i++) {
        for (int j = 0; j < XPT2046_SMPSIZE - 1; j++) {
            if (samples_x[j].y > distances[j + 1].y) {
                int yy = samples_x[j + 1].y;
                samples_x[j + 1].y = samples_x[j].y;
                samples_x[j].y = yy;
                int xx = samples_x[j + 1].x;
                samples_x[j + 1].x = samples_x[j].x;
                samples_x[j].x = xx;
            }
        }
    }
    // sort by distance
    for (int i = 0; i < XPT2046_SMPSIZE - 1; i++) {
        for (int j = 0; j < XPT2046_SMPSIZE - 1; j++) {
            if (samples_y[j].y > distances[j + 1].y) {
                int yy = samples_y[j + 1].y;
                samples_y[j + 1].y = samples_y[j].y;
                samples_y[j].y = yy;
                int xx = samples_y[j + 1].x;
                samples_y[j + 1].x = samples_y[j].x;
                samples_y[j].x = xx;
            }
        }
    }
    int x_test, y_test;
    x_test = 0;
    y_test = 0;

    
    if((samples_x[6].y - samples_x[3].y) > 5){
        m_pressed = false;
        ESP_LOGI(TAG, "X Error");
    }else{
        x_test = 1;
        //ESP_LOGI(TAG, "X OK");
    }
    if((samples_y[6].y - samples_y[3].y) > 5){
        m_pressed = false;
        ESP_LOGI(TAG, "Y Error");
    }else{
        y_test = 1;
        //ESP_LOGI(TAG, "Y OK");
    }

    int tx = 0;
    int ty = 0;
    if((x_test == 1)&(y_test == 1)&((gpio_get_level((gpio_num_t) m_io_irq) == 0))){
        for (int i = 3; i < 7; i++) {
            tx += samples_x[i].y;
            ty += samples_y[i].y;
        }

        tx = tx / 4;
        ty = ty / 4;
        ESP_LOGI(TAG, "*****");
        switch (m_rotation) {
        case 0:
            m_pos.x = tx;
            m_pos.y = ty;
            break;
        case 3:
            m_pos.x = XPT2046_SMP_MAX - ty;
            m_pos.y = tx;
            break;
        case 2:
            m_pos.x = XPT2046_SMP_MAX - tx;
            m_pos.y = XPT2046_SMP_MAX - ty;
            break;
        case 1:
            m_pos.x = ty;
            m_pos.y = XPT2046_SMP_MAX - tx;
            break;
    }
    }else{
       ESP_LOGI(TAG, "###"); 
    }

    //ESP_LOGI(TAG, "X:%d", tx);
    //ESP_LOGI(TAG, "Y:%d", ty);
    //tx = aveX;
    //ty = aveY;    
    
}
*/
/*
void CXpt2046::sample()
{
    position samples[XPT2046_SMPSIZE];
    position distances[XPT2046_SMPSIZE];
    m_pressed = true;

    int tx = 0;
    int ty = 0;
 
    switch (m_rotation) {
        case 0:
            m_pos.x = tx;
            m_pos.y = ty;
            break;
        case 3:
            m_pos.x = XPT2046_SMP_MAX - ty;
            m_pos.y = tx;
            break;
        case 2:
            m_pos.x = XPT2046_SMP_MAX - tx;
            m_pos.y = XPT2046_SMP_MAX - ty;
            break;
        case 1:
            m_pos.x = ty;
            m_pos.y = XPT2046_SMP_MAX - tx;
            break;
    }
}
*/

void CXpt2046::sample()
{
    position samples[XPT2046_SMPSIZE];
    position distances[XPT2046_SMPSIZE];
    m_pressed = true;

    int last_x = 0;
    int last_y = 0;
    int aveX = 0;
    int aveY = 0;

    for (int i = 0; i < XPT2046_SMPSIZE; i++) {
        samples[i].x = get_sample(TOUCH_CMD_X);
        samples[i].y = get_sample(TOUCH_CMD_Y);

        if (samples[i].x == 0 || samples[i].x == XPT2046_SMP_MAX || samples[i].y == 0
                || samples[i].y == XPT2046_SMP_MAX) {
            m_pressed = false;
        }
        aveX += samples[i].x;
        aveY += samples[i].y;
    }
    aveX /= XPT2046_SMPSIZE;
    aveY /= XPT2046_SMPSIZE;
    for (int i = 0; i < XPT2046_SMPSIZE; i++) {
        distances[i].x = i;
        distances[i].y = ((aveX - samples[i].x) * (aveX - samples[i].x))
                         + ((aveY - samples[i].y) * (aveY - samples[i].y));
    }

    // sort by distance
    for (int i = 0; i < XPT2046_SMPSIZE - 1; i++) {
        for (int j = 0; j < XPT2046_SMPSIZE - 1; j++) {
            if (distances[j].y > distances[j + 1].y) {
                int yy = distances[j + 1].y;
                distances[j + 1].y = distances[j].y;
                distances[j].y = yy;
                int xx = distances[j + 1].x;
                distances[j + 1].x = distances[j].x;
                distances[j].x = xx;
            }
        }
    }

    int tx = 0;
    int ty = 0;
    for (int i = 0; i < (XPT2046_SMPSIZE / 2); i++) {
        tx += samples[distances[i].x].x;
        ty += samples[distances[i].x].y;
    }

    tx = tx / (XPT2046_SMPSIZE / 2);
    ty = ty / (XPT2046_SMPSIZE / 2);
    //tx = aveX;
    //ty = aveY;    
    switch (m_rotation) {
        case 0:
            m_pos.x = tx;
            m_pos.y = ty;
            break;
        case 3:
            m_pos.x = XPT2046_SMP_MAX - ty;
            m_pos.y = tx;
            break;
        case 2:
            m_pos.x = XPT2046_SMP_MAX - tx;
            m_pos.y = XPT2046_SMP_MAX - ty;
            break;
        case 1:
            m_pos.x = ty;
            m_pos.y = XPT2046_SMP_MAX - tx;
            break;
    }
}

