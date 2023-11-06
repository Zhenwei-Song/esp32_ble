/*
 * @Author: Zhenwei Song zhenwei.song@qq.com
 * @Date: 2023-10-27 14:17:33
 * @LastEditors: Zhenwei Song zhenwei.song@qq.com
 * @LastEditTime: 2023-10-31 10:33:45
 * @FilePath: \esp32\esp_ble_mesh\ble_mesh_node\onoff_client\main\my_rx.h
 * @Description: 仅供学习交流使用
 * Copyright (c) 2023 by Zhenwei Song, All Rights Reserved.
 */
#ifndef _MY_RX_H_
#define _MY_RX_H_

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "string.h"
#include <stdio.h>

static const int RX_BUF_SIZE = 1024;
extern bool my_led_test;
extern bool msg_detected;
void my_rx_init(void);
void my_rx_task(void *arg);

#endif