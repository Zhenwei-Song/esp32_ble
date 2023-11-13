/*
 * @Author: Zhenwei-Song zhenwei.song@qq.com
 * @Date: 2023-11-08 16:35:47
 * @LastEditors: Zhenwei-Song zhenwei.song@qq.com
 * @LastEditTime: 2023-11-11 10:08:22
 * @FilePath: \esp32\gatt_server_service_table_modified\main\ble_queue.h
 * @Description: 仅供学习交流使用
 * Copyright (c) 2023 by Zhenwei-Song, All Rights Reserved.
 */
#ifndef _BLE_QUEUE_H_
#define _BLE_QUEUE_H_
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_log.h"

#define QUEUE_TAG "QUEUE"
#define queue_data_length 31

typedef struct qnode {
    uint8_t data[queue_data_length];
    struct qnode *next;
} qnode, *p_qnode;

typedef struct queue {
    p_qnode head;
    p_qnode tail;
} queue, *p_queue;

void queue_init(p_queue q);

void queue_push(p_queue q, uint8_t *data);

void queue_push_with_check(p_queue q, uint8_t *data);

uint8_t *queue_pop(p_queue q);

bool queue_is_empty(p_queue q);

void queue_print(p_queue q);

void queue_destroy(p_queue q);

#endif // _BLE_QUEUE_H_