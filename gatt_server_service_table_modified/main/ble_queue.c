/*
 * @Author: Zhenwei-Song zhenwei.song@qq.com
 * @Date: 2023-11-08 16:36:10
 * @LastEditors: Zhenwei-Song zhenwei.song@qq.com
 * @LastEditTime: 2023-11-14 14:32:25
 * @FilePath: \esp32\gatt_server_service_table_modified\main\ble_queue.c
 * @Description: 仅供学习交流使用
 * Copyright (c) 2023 by Zhenwei-Song, All Rights Reserved.
 */
#include "ble_queue.h"

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
    if (new_node == NULL) {
        ESP_LOGE(QUEUE_TAG, "malloc failed");
    }
    else {
        memcpy(new_node->data, data, queue_data_length);
        if (q->head == NULL) { // 队列为空
            q->head = q->tail = new_node;
            // new_node->next = NULL;
            q->tail->next = NULL;
        }
        else {
            q->tail->next = new_node;
            q->tail = new_node;
            // new_node->next = NULL;
            q->tail->next = NULL;
        }
    }
}

/**
 * @description:
 * @param {p_queue} q
 * @param {uint8_t} *data
 * @return {*}
 */
void queue_push_with_check(p_queue q, uint8_t *data)
{
    int repeated = -1;
    if (q->tail == NULL) { // 队列为空
        p_qnode new_node_head = (p_qnode)malloc(sizeof(qnode));
        memcpy(new_node_head->data, data, queue_data_length);
        q->head = q->tail = new_node_head;
        new_node_head->next = NULL;
        q->tail->next = NULL;
        //ESP_LOGW(QUEUE_TAG, "message add to head");
    }
    else {
        repeated = memcmp(data, q->tail->data, queue_data_length);
        if (repeated != 0) { // 检查是否与队列中最后一个数据相同

            p_qnode new_node = (p_qnode)malloc(sizeof(qnode));
            if (new_node == NULL) {
                ESP_LOGE(QUEUE_TAG, "malloc failed");
            }
            else {
                memcpy(new_node->data, data, queue_data_length);
                q->tail->next = new_node;
                q->tail = new_node;
                new_node->next = NULL;
                q->tail->next = NULL;
            }
            //ESP_LOGW(QUEUE_TAG, "message add to queue");
        }
        else {
            // ESP_LOGW(QUEUE_TAG, "message is repeated");
        }
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
        // uint8_t *pop_data = NULL;
        uint8_t *pop_data = (uint8_t *)malloc(sizeof(uint8_t) * queue_data_length);
        if (pop_data == NULL) {
            ESP_LOGE(QUEUE_TAG, "malloc failed");
            return NULL;
        }
        else {
            // pop_data = q->head->data;
            memcpy(pop_data, q->head->data, queue_data_length);
            p_qnode temp = q->head;
            q->head = q->head->next;
            free(temp);
            return pop_data;
        }
    }
    else { // 队列为空
        ESP_LOGW(QUEUE_TAG, "pop NULL");
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
 * @description: 打印队列(有问题)
 * @param {p_queue} q
 * @return {*}
 */
void queue_print(p_queue q)
{
    p_qnode node = q->head;
    ESP_LOGW(QUEUE_TAG, "********************************Start printing queue:********************************************************");
    while (node != NULL) {
        esp_log_buffer_hex(QUEUE_TAG, node->data, queue_data_length);
        node = node->next;
    }
    ESP_LOGW(QUEUE_TAG, "********************************Printing queue is finished***************************************************");
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