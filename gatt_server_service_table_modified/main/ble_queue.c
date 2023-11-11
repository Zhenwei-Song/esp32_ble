/*
 * @Author: Zhenwei-Song zhenwei.song@qq.com
 * @Date: 2023-11-08 16:36:10
 * @LastEditors: Zhenwei-Song zhenwei.song@qq.com
 * @LastEditTime: 2023-11-11 10:56:10
 * @FilePath: \esp32\gatt_server_service_table_modified\main\ble_queue.c
 * @Description: 仅供学习交流使用
 * Copyright (c) 2023 by Zhenwei-Song, All Rights Reserved.
 */
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ble_queue.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_log.h"

/**
 * @description: 初始化队列结构
 * @param {queue} *q
 * @return {*}
 */
void queue_init(p_queue q)
{
    q->head = NULL;
    q->tail = NULL;
}

/**
 * @description: 队列尾部插入数据
 * @param {queue} *q
 * @param {uint8_t} data
 * @return {*}
 */
void queue_push(p_queue q, uint8_t *data)
{
    p_qnode new_node = (p_qnode)malloc(sizeof(qnode));
    memcpy(new_node->data, data, 31);
    if (q->head == NULL) { // 队列为空
        q->head = q->tail = new_node;
        q->tail->next = NULL;
    }
    else {
        q->tail->next = new_node;
        q->tail = new_node;
        q->tail->next = NULL;
    }
}

void queue_push_with_check(p_queue q, uint8_t *data)
{
    if (memcmp(q->tail->data, data, 31) != 0) { // 检查是否与队列中最后一个数据相同

        p_qnode new_node = (p_qnode)malloc(sizeof(qnode));
        memcpy(new_node->data, data, 31);
        if (q->head == NULL) { // 队列为空
            q->head = q->tail = new_node;
            q->tail->next = NULL;
        }
        else {
            q->tail->next = new_node;
            q->tail = new_node;
            q->tail->next = NULL;
        }
    }
    else {
        ESP_LOGW(QUEUE_TAG, "message is repeated");
    }
}
/**
 * @description: 队列头出数据
 * @param {p_queue} q
 * @return {*}
 */
uint8_t *queue_pop(p_queue q)
{
    if (q->head != NULL) {
        uint8_t *pop_data = (uint8_t *)malloc(31 * sizeof(uint8_t));
        memcpy(pop_data, q->head->data, 31);
        p_qnode temp = q->head;
        q->head = q->head->next;
        free(temp);
        return pop_data;
    }
    else { // 队列为空
        return NULL;
    }
}

/**
 * @description: 判断队列是否为空
 * @param {p_queue} q
 * @return {*} 1：队列为空；0：队列非空
 */
bool queue_is_empty(p_queue q)
{
    if (q->head == NULL)
        return true;
    else
        return false;
}

/**
 * @description: 打印队列
 * @param {p_queue} q
 * @return {*}
 */
void queue_print(p_queue q)
{
    p_qnode node = q->head;
    uint8_t data_size = strlen((char *)q->head->data);
    if (q->head == NULL) {
        ESP_LOGW(QUEUE_TAG, "queue is empty");
    }
    else {
        ESP_LOGW(QUEUE_TAG, "********************************Start printing queue:********************************************************");
        while (node != NULL) {
            esp_log_buffer_hex(QUEUE_TAG, node->data, data_size);
            node = node->next;
        }
        ESP_LOGW(QUEUE_TAG, "********************************Printing queue is finished***************************************************");
    }
}

/**
 * @description: 销毁队列
 * @param {p_queue} q
 * @return {*}
 */
void queue_destroy(p_queue q)
{
    ESP_LOGW(QUEUE_TAG, "Destroying queue!");
    while (q->head != NULL)
        queue_pop(q);
    ESP_LOGW(QUEUE_TAG, "Destroying queue finished!");
}