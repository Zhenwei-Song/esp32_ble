/*
 * @Author: Zhenwei-Song zhenwei.song@qq.com
 * @Date: 2023-11-09 15:05:15
 * @LastEditors: Zhenwei-Song zhenwei.song@qq.com
 * @LastEditTime: 2023-11-16 10:54:20
 * @FilePath: \esp32\gatt_server_service_table_modified\main\routing_table.c
 * @Description: 仅供学习交流使用
 * Copyright (c) 2023 by Zhenwei-Song, All Rights Reserved.
 */

#include "esp_log.h"

#include "routing_table.h"

routing_table neighbor_table;
/**
 * @description: 初始化路由表
 * @param {p_routing_table} table
 * @return {*}
 */
void init_routing_table(p_routing_table table)
{
    table->head = NULL;
}

/**
 * @description: 向路由表添加节点
 * @param {p_routing_table} table
 * @param {uint8_t} *new_id
 * @param {bool} is_root
 * @param {bool} is_connected
 * @param {uint8_t} *quality
 * @param {uint8_t} distance
 * @return {*}
 */
int insert_routing_node(p_routing_table table, uint8_t *new_id, bool is_root, bool is_connected, uint8_t *quality, uint8_t distance)
{
    p_routing_note new_node = (p_routing_note)malloc(sizeof(routing_note));
    if (new_node == NULL) {
        ESP_LOGE(ROUTING_TAG, "malloc failed");
        return -1;
    }
    memcpy(new_node->id, new_id, ID_LEN);
    new_node->is_root = is_root;
    new_node->is_connected = is_connected;
    memcpy(new_node->quality, quality, QUALITY_LEN);
    new_node->distance = distance;
    new_node->count = ROUTING_TABLE_COUNT;
    p_routing_note cur = table->head;
    int repeated = -1;

    if (table->head == NULL) { // 路由表为空
        table->head = new_node;
        new_node->next = NULL;
    }
    else {
        while (cur != NULL) {
            // ESP_LOGI(ROUTING_TAG, "table->head->id addr: %p", cur->id);
            repeated = memcmp(cur->id, new_node->id, ID_LEN);
            if (repeated == 0) { // 检查重复,重复则更新
                                 // ESP_LOGI(ROUTING_TAG, "repeated id address found");
                cur->is_root = new_node->is_root;
                cur->is_connected = new_node->is_connected;
                memcpy(cur->quality, new_node->quality, QUALITY_LEN);
                cur->distance = new_node->distance;
                cur->count = new_node->count; // 重新计数
                free(new_node);
                goto exit;
            }
            if (cur->next == NULL) // 链表中的最后一个节点
                break;
            cur = cur->next;
        }
        cur->next = new_node; // 表尾添加新项
        new_node->next = NULL;
    }
    ESP_LOGW(ROUTING_TAG, "ADD NEW NOTE");
exit:
    return 0;
}

/**
 * @description:从路由链表移除项
 * @param {p_routing_table} table
 * @param {p_routing_note} old_routing
 * @return {*}
 */
void remove_routing_node_from_node(p_routing_table table, p_routing_note old_routing)
{
    if (table->head != NULL) {
        p_routing_note prev = NULL;
        if (table->head == old_routing) { // 移除头部
            table->head = table->head->next;
            free(old_routing);
        }
        else { // 找出old_routing的上一项
            prev = table->head;
            while (prev != NULL) {
                if (prev->next == old_routing) // 找到要删除的项
                    break;
                else
                    prev = prev->next;
            }
            if (prev != NULL)
                free(prev->next);
            prev->next = old_routing->next;
        }
    }
}

/**
 * @description: 从路由表移除项（根据id）
 * @param {p_routing_table} table
 * @param {uint8_t} *old_id
 * @return {*}
 */
void remove_routing_node(p_routing_table table, uint8_t *old_id)
{
    if (table->head != NULL) {
        p_routing_note prev = NULL;
        if (memcmp(table->head->id, old_id, ID_LEN) == 0) { // 移除头部
            prev = table->head;
            table->head = table->head->next;
            free(prev);
        }
        else { // 找出old_routing的上一项
            prev = table->head;
            while (prev != NULL) {
                if (prev->next->id == old_id) // 找到要删除的项
                    break;
                else
                    prev = prev->next;
            }
            if (prev != NULL) {
                prev->next = prev->next->next;
                free(prev->next);
            }
        }
    }
}

/**
 * @description: 判断路由表是否为空
 * @param {p_routing_table} table
 * @return {*}true：为空    false：非空
 */
bool is_routing_table_empty(p_routing_table table)
{
    if (table->head == NULL)
        return true;
    else
        return false;
}

/**
 * @description: 检查路由表中是否有相同项
 * @param {p_routing_table} table
 * @param {p_routing_note} routing_note
 * @return {*} true:有相同项;false：无相同项
 */
bool routing_table_check_id(p_routing_table table, uint8_t *id)
{
    p_routing_note temp = table->head;
    if (temp == NULL) {
        return false;
    }
    else {
        while (temp != NULL) {
            if (memcmp(temp->id, id, ID_LEN) == 0)
                return true;
            else
                temp = temp->next;
        }
        return false;
    }
}

/**
 * @description: 设置特定id的distance
 * @param {p_routing_table} table
 * @param {uint8_t} *id
 * @param {uint8_t} distance
 * @return {*}0:成功
 */
int set_routing_node_distance(p_routing_table table, uint8_t *id, int8_t distance)
{
    p_routing_note temp = table->head;
    if (temp == NULL) {
        return -1;
    }
    else {
        while (temp != NULL) {
            if (memcmp(temp->id, id, ID_LEN) == 0) { // 找到了
                temp->distance = distance;
                return 0;
            }
            else {
                temp = temp->next;
            }
        }
        return -1;
    }
}

/**
 * @description:获取特定id的distance
 * @param {p_routing_table} table
 * @param {uint8_t} *id
 * @return {*}成功：返回distance
 */
int8_t get_routing_node_distance(p_routing_table table, uint8_t *id)
{
    p_routing_note temp = table->head;
    if (temp == NULL) {
        return -1;
    }
    else {
        while (temp != NULL) {
            if (memcmp(temp->id, id, ID_LEN) == 0) { // 找到了
                return temp->distance;
            }
            else {
                temp = temp->next;
            }
        }
        return -1;
    }
}

/**
 * @description: 更新路由表的计数号（-1）
 * @param {p_routing_table} table
 * @return {*}
 */
void refresh_cnt_routing_table(p_routing_table table)
{
    if (table->head != NULL) {
        p_routing_note temp = table->head;
        while (temp != NULL) {
            if (temp->count == 0) {
                remove_routing_node_from_node(table, temp);
                ESP_LOGW(ROUTING_TAG, "routing table node deleted");
            }
            else {
                temp->count = temp->count - 1;
                // ESP_LOGW(ROUTING_TAG, "routing table refreshed");
            }
            temp = temp->next;
            if (temp == NULL)
                break;
        }
        print_routing_table(table);
    }
}
/**
 * @description: 打印邻居表
 * @param {p_routing_table} table
 * @return {*}
 */
void print_routing_table(p_routing_table table)
{
    p_routing_note temp = table->head;
    ESP_LOGI(ROUTING_TAG, "****************************Start printing routing table:***********************************************");
    while (temp != NULL) {
        ESP_LOGI(ROUTING_TAG, "id:");
        esp_log_buffer_hex(ROUTING_TAG, temp->id, ID_LEN);
        ESP_LOGI(ROUTING_TAG, "is_root:%d", temp->is_root);
        ESP_LOGI(ROUTING_TAG, "is_connected:%d", temp->is_connected);
        ESP_LOGI(ROUTING_TAG, "quality:");
        esp_log_buffer_hex(ROUTING_TAG, temp->quality, QUALITY_LEN);
        ESP_LOGI(ROUTING_TAG, "distance:%d", temp->distance);
        ESP_LOGI(ROUTING_TAG, "count:%d", temp->count);
        temp = temp->next;
    }
    ESP_LOGI(ROUTING_TAG, "****************************Printing routing table is finished *****************************************");
}

/**
 * @description: 销毁路由表
 * @param {p_routing_table} table
 * @return {*}
 */
void destroy_routing_table(p_routing_table table)
{
    ESP_LOGW(ROUTING_TAG, "Destroying routing_table!");
    while (table->head != NULL)
        remove_routing_node_from_node(table, table->head);
    ESP_LOGW(ROUTING_TAG, "Destroying routing_table finished!");
}