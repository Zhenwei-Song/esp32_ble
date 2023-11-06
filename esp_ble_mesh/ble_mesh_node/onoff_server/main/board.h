/*
 * @Author: Zhenwei Song zhenwei.song@qq.com
 * @Date: 2023-09-07 16:09:10
 * @LastEditors: Zhenwei Song zhenwei.song@qq.com
 * @LastEditTime: 2023-09-14 14:57:26
 * @FilePath: \blee:\Espressif\frameworks\esp-idf-v5.0.2\examples\bluetooth\esp_ble_mesh\ble_mesh_node\onoff_server\main\board.h
 * @Description: 仅供学习交流使用
 * Copyright (c) 2023 by Zhenwei Song, All Rights Reserved.
 */
/* board.h - Board-specific hooks */

/*
 * SPDX-FileCopyrightText: 2017 Intel Corporation
 * SPDX-FileContributor: 2018-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _BOARD_H_
#define _BOARD_H_

#include "driver/gpio.h"

#if defined(CONFIG_BLE_MESH_ESP_WROOM_32)
#define LED_R GPIO_NUM_25
#define LED_G GPIO_NUM_26
#define LED_B GPIO_NUM_27
#elif defined(CONFIG_BLE_MESH_ESP_WROVER)
#define LED_R GPIO_NUM_0
#define LED_G GPIO_NUM_2
#define LED_B GPIO_NUM_4
#elif defined(CONFIG_BLE_MESH_ESP32C3_DEV)
#define LED_R GPIO_NUM_8
#define LED_G GPIO_NUM_8
#define LED_B GPIO_NUM_8
#elif defined(CONFIG_BLE_MESH_ESP32S3_DEV)
#define LED_R GPIO_NUM_47
#define LED_G GPIO_NUM_47
#define LED_B GPIO_NUM_47
#endif

#define LED_ON 1
#define LED_OFF 0

struct _led_state {
    uint8_t current;
    uint8_t previous;
    uint8_t pin;
    char *name;
};

void board_led_operation(uint8_t pin, uint8_t onoff);

void board_init(void);

#endif
