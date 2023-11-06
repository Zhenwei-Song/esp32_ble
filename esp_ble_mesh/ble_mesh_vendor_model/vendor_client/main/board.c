/*
 * @Author: Zhenwei-Song zhenwei.song@qq.com
 * @Date: 2023-10-30 20:37:31
 * @LastEditors: Zhenwei-Song zhenwei.song@qq.com
 * @LastEditTime: 2023-11-03 20:23:55
 * @FilePath: \esp32\esp_ble_mesh\ble_mesh_vendor_model\vendor_client\main\board.c
 * @Description: 仅供学习交流使用
 * Copyright (c) 2023 by Zhenwei-Song, All Rights Reserved.
 */
/* board.c - Board-specific hooks */

/*
 * SPDX-FileCopyrightText: 2017 Intel Corporation
 * SPDX-FileContributor: 2018-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "board.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "iot_button.h"
#include <stdio.h>

#define TAG "BOARD"

#define BUTTON_IO_NUM 0
#define BUTTON_ACTIVE_LEVEL 0

struct _led_state led_state[3] = {
    {LED_OFF, LED_OFF, LED_R, "red"},
    {LED_OFF, LED_OFF, LED_G, "green"},
    {LED_OFF, LED_OFF, LED_B, "blue"},
};

extern void example_ble_mesh_send_vendor_message(bool resend);

static void button_tap_cb(void *arg)
{
    example_ble_mesh_send_vendor_message(false);
}

static void board_button_init(void)
{
    button_handle_t btn_handle = iot_button_create(BUTTON_IO_NUM, BUTTON_ACTIVE_LEVEL);
    if (btn_handle) {
        iot_button_set_evt_cb(btn_handle, BUTTON_CB_RELEASE, button_tap_cb, "RELEASE");
    }
}

void board_led_operation(uint8_t pin, uint8_t onoff)
{
    for (int i = 0; i < 3; i++) {
        if (led_state[i].pin != pin) {
            continue;
        }
        if (onoff == led_state[i].previous) {
            ESP_LOGW(TAG, "led %s is already %s",
                     led_state[i].name, (onoff ? "on" : "off"));
            return;
        }
        gpio_set_level(pin, onoff);
        led_state[i].previous = onoff;
        return;
    }

    ESP_LOGE(TAG, "LED is not found!");
}

static void board_led_init(void)
{
    for (int i = 0; i < 3; i++) {
        gpio_reset_pin(led_state[i].pin);
        gpio_set_direction(led_state[i].pin, GPIO_MODE_OUTPUT);
        gpio_set_level(led_state[i].pin, LED_OFF);
        led_state[i].previous = LED_OFF;
    }
}

void board_init(void)
{
    board_button_init();
    board_led_init();
}
