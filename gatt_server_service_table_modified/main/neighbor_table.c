/*
 * @Author: Zhenwei-Song zhenwei.song@qq.com
 * @Date: 2023-11-09 15:05:15
 * @LastEditors: Zhenwei-Song zhenwei.song@qq.com
 * @LastEditTime: 2023-12-04 20:19:31
 * @FilePath: \esp32\gatt_server_service_table_modified\main\neighbor_table.c
 * @Description: 仅供学习交流使用
 * Copyright (c) 2023 by Zhenwei-Song, All Rights Reserved.
 */

#include "esp_log.h"

#include "ble_queue.h"
#include "neighbor_table.h"

bool refresh_flag_for_neighbor = false;

bool threshold_high_flag = false;

bool threshold_low_flag = false;

neighbor_table my_neighbor_table;

/**
 * @description: 初始化路由表
 * @param {p_neighbor_table} table
 * @return {*}
 */
void init_neighbor_table(p_neighbor_table table)
{
    table->head = NULL;
}

/**
 * @description: 向路由表添加节点
 * @param {p_neighbor_table} table
 * @param {uint8_t} *new_id
 * @param {bool} is_root
 * @param {bool} is_connected
 * @param {uint8_t} *quality
 * @param {uint8_t} distance
 * @return {*}
 */
int insert_neighbor_node(p_neighbor_table table, uint8_t *new_id, bool is_root, bool is_connected, uint8_t *quality, uint8_t distance, int rssi)
{
    p_neighbor_note new_node = (p_neighbor_note)malloc(sizeof(neighbor_note));
    p_neighbor_note prev = NULL;
    if (new_node == NULL) {
        ESP_LOGE(NEIGHBOR_TAG, "malloc failed");
        return -1;
    }
    memcpy(new_node->id, new_id, ID_LEN);
    new_node->is_root = is_root;
    new_node->is_connected = is_connected;
    memcpy(new_node->quality, quality, QUALITY_LEN);
    new_node->distance = distance;
    new_node->rssi = rssi;
    // new_node->quality = quality;
    new_node->count = NEIGHBOR_TABLE_COUNT;
    p_neighbor_note cur = table->head;
    int repeated = -1;
    if (table->head == NULL) { // 路由表为空
        table->head = new_node;
        new_node->next = NULL;
        return 0;
    }
    else {
        while (cur != NULL) {
            // ESP_LOGI(NEIGHBOR_TAG, "table->head->id addr: %p", cur->id);
            repeated = memcmp(cur->id, new_node->id, ID_LEN);
            if (repeated == 0) { // 检查重复,重复则更新
                // ESP_LOGI(NEIGHBOR_TAG, "repeated id address found");
                cur->is_root = new_node->is_root;
                cur->is_connected = new_node->is_connected;
                memcpy(cur->quality, new_node->quality, QUALITY_LEN);
                // cur->quality = new_node->quality;
                cur->distance = new_node->distance;
                cur->rssi = new_node->rssi;
                cur->count = new_node->count; // 重新计数
                free(new_node);
                return 0;
            }
            prev = cur;
            cur = cur->next;
        }
        if (prev != NULL) {
            prev->next = new_node; // 表尾添加新项
            new_node->next = NULL;
        }
    }
    ESP_LOGW(NEIGHBOR_TAG, "ADD NEW NEIGHBOT NOTE");
    // print_neighbor_table(table);
    return 0;
}

/**
 * @description:从路由链表移除项
 * @param {p_neighbor_table} table
 * @param {p_neighbor_note} old_neighbor
 * @return {*}
 */
void remove_neighbor_node_from_node(p_neighbor_table table, p_neighbor_note old_neighbor)
{
    if (table->head != NULL) {
        p_neighbor_note prev = NULL;
        if (table->head == old_neighbor) { // 移除头部
            table->head = table->head->next;
            free(old_neighbor);
        }
        else { // 找出old_neighbor的上一项
            prev = table->head;
            while (prev != NULL) {
                if (prev->next == old_neighbor) // 找到要删除的项
                    break;
                else
                    prev = prev->next;
            }
            if (prev != NULL) {
                prev->next = old_neighbor->next;
                free(old_neighbor);
            }
        }
    }
}

/**
 * @description: 从路由表移除项（根据id）
 * @param {p_neighbor_table} table
 * @param {uint8_t} *old_id
 * @return {*}
 */
void remove_neighbor_node(p_neighbor_table table, uint8_t *old_id)
{
    if (table->head != NULL) {
        p_neighbor_note prev = NULL;
        if (memcmp(table->head->id, old_id, ID_LEN) == 0) { // 移除头部
            prev = table->head;
            table->head = table->head->next;
            free(prev);
        }
        else { // 找出old_neighbor的上一项
            prev = table->head;
            while (prev != NULL) {
                if (prev->next->id == old_id) // 找到要删除的项
                    break;
                else
                    prev = prev->next;
            }
            if (prev != NULL) {
                p_neighbor_note old_neighbor = prev->next;
                prev->next = prev->next->next;
                free(old_neighbor);
            }
        }
    }
}

/**
 * @description: 判断路由表是否为空
 * @param {p_neighbor_table} table
 * @return {*}true：为空    false：非空
 */
bool is_neighbor_table_empty(p_neighbor_table table)
{
    if (table->head == NULL)
        return true;
    else
        return false;
}

/**
 * @description: 检查路由表中是否有相同项
 * @param {p_neighbor_table} table
 * @param {p_neighbor_note} neighbor_note
 * @return {*} true:有相同项;false：无相同项
 */
bool neighbor_table_check_id(p_neighbor_table table, uint8_t *id)
{
    p_neighbor_note temp = table->head;
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
#if 0
void update_my_connection_info(p_neighbor_table table, p_my_info my_info)
{
    my_info->able_to_connect = false;
    if (!is_neighbor_table_empty(table)) { // 邻居表非空
        p_neighbor_note temp_node;
        temp_node = table->head;
        while (temp_node != NULL) {
            if (temp_node->is_connected) {
                my_info->able_to_connect = true;
                my_info->is_connected |= 1;
                if (temp_node->quality[0] < my_info->quality[0] || (temp_node->quality[0] == my_info->quality[0] && temp_node->quality[1] < my_info->quality[1])) {
                    memcpy(my_info->next_id, temp_node->id, ID_LEN);
                    memcpy(my_info->quality, temp_node->quality, QUALITY_LEN);
                }
            }
            temp_node = temp_node->next;
        }
        ESP_LOGE(DATA_TAG, "update_my_connection_info finished");
    }
}
#endif
/**
 * @description: 设置特定id的distance
 * @param {p_neighbor_table} table
 * @param {uint8_t} *id
 * @param {uint8_t} distance
 * @return {*}0:成功
 */
int set_neighbor_node_distance(p_neighbor_table table, uint8_t *id, int8_t distance)
{
    p_neighbor_note temp = table->head;
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
 * @param {p_neighbor_table} table
 * @param {uint8_t} *id
 * @return {*}成功：返回distance
 */
uint8_t get_neighbor_node_distance(p_neighbor_table table, uint8_t *id)
{
    p_neighbor_note temp = table->head;
    if (temp == NULL) {
        return 0;
    }
    else {
        while (temp != NULL) {
            if (memcmp(temp->id, id, ID_LEN) == 0) { // 找到了
                return temp->distance;
            }
            temp = temp->next;
        }
        return 0;
    }
}

#if 0
/**
 * @description: 更新路由表的计数号（-1）
 * @param {p_neighbor_table} table
 * @return {*}
 */
void refresh_cnt_neighbor_table(p_neighbor_table table, p_my_info info)
{
    if (table->head != NULL) {
        p_neighbor_note temp = table->head;
        while (temp != NULL) {
            if (temp->count == 0) {
                if (temp->is_root == true) { // 若表中的root节点无了，更新自己info
                    info->update = info->update + 1;
                    info->is_connected = false;
                    info->distance = 100;
                    memset(info->root_id, 0, ID_LEN);
                    memset(info->next_id, 0, ID_LEN);
                    refresh_flag_for_neighbor = true;
                    ESP_LOGE(NEIGHBOR_TAG, "root deleted");
                }
                p_neighbor_note next_temp = temp->next; // 保存下一个节点以防止删除后丢失指针
                remove_neighbor_node_from_node(table, temp);
                temp = next_temp; // 更新temp为下一个节点
                ESP_LOGW(NEIGHBOR_TAG, "neighbor table node deleted");
            }
            else {
                temp->count = temp->count - 1;
                temp = temp->next; // 继续到下一个节点
                // ESP_LOGW(NEIGHBOR_TAG, "neighbor table refreshed");
            }
        }
        // update_my_connection_info(table, info);
        print_neighbor_table(table);
    }
}
#else
/**
 * @description: 更新路由表的计数号（-1）
 * @param {p_neighbor_table} table
 * @return {*}
 */
void refresh_cnt_neighbor_table(p_neighbor_table table, p_my_info info)
{
    if (table->head != NULL) {
        p_neighbor_note temp = table->head;
        while (temp != NULL) {
            if (temp->count == 0) {
#ifndef SELF_ROOT
                if (temp->id == info->next_id) { // 若自己的父节点无了，更新自己info
                    // info->update = info->update + 1;
                    info->is_connected = false;
                    info->distance = 100;
                    info->quality[0] = NOR_NODE_INIT_QUALITY;
                    memset(info->root_id, 0, ID_LEN);
                    memset(info->next_id, 0, ID_LEN);
                    refresh_flag_for_neighbor = true;
                    // update_my_connection_info(table, info);
                    ESP_LOGE(NEIGHBOR_TAG, "father deleted");
                }
#endif
                p_neighbor_note next_temp = temp->next; // 保存下一个节点以防止删除后丢失指针
                remove_neighbor_node_from_node(table, temp);
                temp = next_temp; // 更新temp为下一个节点
                ESP_LOGW(NEIGHBOR_TAG, "neighbor table node deleted");
            }
            else {
                temp->count = temp->count - 1;
                temp = temp->next; // 继续到下一个节点
                // ESP_LOGW(NEIGHBOR_TAG, "neighbor table refreshed");
            }
        }
        // print_neighbor_table(table);
    }
    else { // 路由表里一个节点都没有
#ifndef SELF_ROOT
        info->is_connected = false;
        info->distance = 100;
        info->quality[0] = NOR_NODE_INIT_QUALITY;
        memset(info->root_id, 0, ID_LEN);
        memset(info->next_id, 0, ID_LEN);
        refresh_flag_for_neighbor = true;
#endif
    }
}
#endif

void update_quality_of_neighbor_table(p_neighbor_table table)
{

    if (table->head != NULL) {
        p_neighbor_note temp = table->head;
        while (temp != NULL) {
            if (temp->is_connected == true) { // 若为入网节点
                memcpy(temp->quality_from_me, quality_calculate(temp->rssi, temp->quality, temp->distance), QUALITY_LEN);
                if (memcmp(temp->quality_from_me, threshold_high, QUALITY_LEN) >= 0 && temp->is_root == true) { // 大于阈值1，且为root
                    threshold_high_flag |= 1;
                    threshold_low_flag |= 0;
                }
                else if (memcmp(temp->quality_from_me, threshold_low, QUALITY_LEN) >= 0) { // 大于阈值2,小于阈值1
                    threshold_high_flag |= 0;
                    threshold_low_flag |= 1;
                }
                else { // 小于阈值2
                    threshold_high_flag |= 0;
                    threshold_low_flag |= 0;
                }
            }
            temp = temp->next; // 继续到下一个节点
        }
        // print_neighbor_table(table);
    }
    else { // 路由表为空
        threshold_high_flag = 0;
        threshold_low_flag = 0;
    }
}

#if 0
void threshold_high_ops(p_neighbor_table table, p_my_info info)
{
    if (table->head != NULL) {
        p_neighbor_note temp = table->head;
        while (temp != NULL) {
            if (temp->is_connected == true) {                                          // 若为入网节点
                if (memcmp(temp->quality_from_me, threshold_high, QUALITY_LEN) >= 0) { // 大于阈值1，直接入网
                    info->is_connected |= 1;
                    if (memcmp(info->quality, temp->quality_from_me, QUALITY_LEN) < 0) { // 若有节点的链路质量比自己当前的链路质量高
                        if (memcmp(info->next_id, temp->id, ID_LEN) == 0) {              // 若它就是自己的next id，则更新自己的链路质量
                            memcpy(info->quality, temp->quality_from_me, QUALITY_LEN);
                        }
                        else { // 更新自己的next_id
                            memcpy(info->next_id, temp->id, ID_LEN);
                            info->distance = temp->distance + 1;
                            refresh_flag_for_neighbor = true; // 因为自己的next_id变了，所以立即广播hello
                            memcpy(info->quality, temp->quality_from_me, QUALITY_LEN);
                        }
                    }
                }
            }
            temp = temp->next; // 继续到下一个节点
        }
        // print_neighbor_table(table);
    }
}
#else
void threshold_high_ops(p_neighbor_table table, p_my_info info)
{
    if (table->head != NULL) {
        p_neighbor_note temp = table->head;
        while (temp != NULL) {
            if (temp->is_connected == true) {                                          // 若为入网节点
                if (memcmp(temp->quality_from_me, threshold_high, QUALITY_LEN) >= 0) { // 大于阈值1，直接入网
                    info->is_connected |= 1;
                    if (memcmp(info->quality, temp->quality_from_me, QUALITY_LEN) < 0) { // 若有节点的链路质量比自己当前的链路质量高
                        if (memcmp(info->next_id, temp->id, ID_LEN) == 0) {              // 若它就是自己的next id，则更新自己的链路质量
                            memcpy(info->quality, temp->quality_from_me, QUALITY_LEN);
                        }
                        else { // 更新自己的next_id
                            memcpy(info->next_id, temp->id, ID_LEN);
                            info->distance = temp->distance + 1;
                            refresh_flag_for_neighbor = true; // 因为自己的next_id变了，所以立即广播hello
                            memcpy(info->quality, temp->quality_from_me, QUALITY_LEN);
                        }
                    }
                }
            }
            temp = temp->next; // 继续到下一个节点
        }
        // print_neighbor_table(table);
    }
}
#endif

void threshold_between_ops(p_neighbor_table table, p_my_info info)
{
    if (table->head != NULL) {
        p_neighbor_note temp = table->head;
        while (temp != NULL) {
            if (temp->is_connected == true) {                                                                                                           // 若为入网节点
                if (memcmp(temp->quality_from_me, threshold_low, QUALITY_LEN) >= 0 && memcmp(temp->quality_from_me, threshold_high, QUALITY_LEN) < 0) { // 大于阈值2，发送ANHSP
                    if (memcmp(info->quality, temp->quality_from_me, QUALITY_LEN) < 0) {                                                                // 若有节点的链路质量比自己当前的链路质量高
                        if (memcmp(info->next_id, temp->id, ID_LEN) == 0) {                                                                             // 若它就是自己的next id，则更新自己的链路质量
                            memcpy(info->quality, temp->quality_from_me, QUALITY_LEN);
                        }
                        else { // 更新自己的next_id
                            memcpy(info->next_id, temp->id, ID_LEN);
                            memcpy(info->quality, temp->quality_from_me, QUALITY_LEN);
                        }
                    }
                }
            }
            temp = temp->next; // 继续到下一个节点
        }
        /* -------------------------------------------------------------------------- */
        /*                               向next_id发送入网请求包                              */
        /* -------------------------------------------------------------------------- */
        memcpy(adv_data_final_for_anhsp, data_match(adv_data_name_7, generate_anhsp(info), HEAD_DATA_LEN, ANHSP_FINAL_DATA_LEN), FINAL_DATA_LEN);
        queue_push(&send_queue, adv_data_final_for_anhsp, 0);
        xSemaphoreGive(xCountingSemaphore_send);
        // print_neighbor_table(table);
    }
}

#if 0
void threshold_low_ops(p_neighbor_table table, p_my_info info)
{
    if (table->head != NULL) {
        p_neighbor_note temp = table->head;
        while (temp != NULL) {
            if (temp->is_connected == true) {                                                                                                           // 若为入网节点
                if (memcmp(temp->quality_from_me, threshold_low, QUALITY_LEN) >= 0 && memcmp(temp->quality_from_me, threshold_high, QUALITY_LEN) < 0) { // 大于阈值2，发送ANHSP

                    if (memcmp(info->next_id, temp->id, ID_LEN) == 0) { // 若它就是自己的next id，则更新自己的链路质量
                        memcpy(info->quality, temp->quality_from_me, QUALITY_LEN);
                    }
                    if (memcmp(info->quality, temp->quality_from_me, QUALITY_LEN) > 0) { // 若有节点的链路质量比自己当前的链路质量高
                        if (memcmp(info->next_id, temp->id, ID_LEN) != 0) {              // 更新自己的next_id
                            memcpy(info->next_id, temp->id, ID_LEN);
                        }
                        memcpy(info->quality, temp->quality_from_me, QUALITY_LEN);
                    }
                }
            }
            temp = temp->next; // 继续到下一个节点
        }
        /* -------------------------------------------------------------------------- */
        /*                               向next_id发送入网请求包                              */
        /* -------------------------------------------------------------------------- */
        memcpy(adv_data_final_for_anrreq, data_match(adv_data_name_7, generate_anrreq(info), HEAD_DATA_LEN, ANHSP_FINAL_DATA_LEN), FINAL_DATA_LEN);
        queue_push(&send_queue, adv_data_final_for_anhsp, 0);
        // print_neighbor_table(table);
    }
}
#endif
void set_my_next_id_quality_and_distance(p_neighbor_table table, p_my_info info)
{
    if (table->head != NULL) {
        p_neighbor_note temp = table->head;
        while (temp != NULL) {
            if (temp->is_connected == true) {                                          // 若为入网节点
                if (memcmp(temp->quality_from_me, threshold_high, QUALITY_LEN) >= 0) { // 大于阈值1，直接入网
                    info->is_connected |= 1;
                    if (memcmp(info->next_id, temp->id, ID_LEN) == 0) { // 若它就是自己的next id，则更新自己的链路质量
                        memcpy(info->quality, temp->quality_from_me, QUALITY_LEN);
                    }
                    if (memcmp(info->quality, temp->quality_from_me, QUALITY_LEN) > 0) { // 若有邻居节点的链路质量比自己当前的链路质量高
                        if (memcmp(info->next_id, temp->id, ID_LEN) != 0) {              // 更新了自己的next_id
                            memcpy(info->next_id, temp->id, ID_LEN);
                            info->distance = temp->distance + 1;
                            refresh_flag_for_neighbor = true; // 因为自己的next_id变了，所以立即广播hello
                        }
                        memcpy(info->quality, temp->quality_from_me, QUALITY_LEN);
                    }
                }
                else if (memcmp(temp->quality_from_me, threshold_low, QUALITY_LEN) >= 0) { // 小于阈值1大于阈值2，发送入网请求
                }
            }
            temp = temp->next; // 继续到下一个节点
        }
        // print_neighbor_table(table);
    }
}

/**
 * @description: 打印邻居表
 * @param {p_neighbor_table} table
 * @return {*}
 */
void print_neighbor_table(p_neighbor_table table)
{
    p_neighbor_note temp = table->head;
    ESP_LOGI(NEIGHBOR_TAG, "****************************Start printing neighbor table:***********************************************");
    while (temp != NULL) {
        ESP_LOGI(NEIGHBOR_TAG, "id:");
        esp_log_buffer_hex(NEIGHBOR_TAG, temp->id, ID_LEN);
        ESP_LOGI(NEIGHBOR_TAG, "is_root:%d", temp->is_root);
        ESP_LOGI(NEIGHBOR_TAG, "is_connected:%d", temp->is_connected);
        ESP_LOGI(NEIGHBOR_TAG, "quality:");
        esp_log_buffer_hex(NEIGHBOR_TAG, temp->quality, QUALITY_LEN);
#ifndef SELF_ROOT
        ESP_LOGI(NEIGHBOR_TAG, "quality from me:");
        esp_log_buffer_hex(NEIGHBOR_TAG, temp->quality_from_me, QUALITY_LEN);
#endif
        ESP_LOGI(NEIGHBOR_TAG, "distance:%d", temp->distance);
        // ESP_LOGI(NEIGHBOR_TAG, "count:%d", temp->count);
        temp = temp->next;
    }
    ESP_LOGI(NEIGHBOR_TAG, "****************************Printing neighbor table is finished *****************************************");
}

/**
 * @description: 销毁路由表
 * @param {p_neighbor_table} table
 * @return {*}
 */
void destroy_neighbor_table(p_neighbor_table table)
{
    ESP_LOGW(NEIGHBOR_TAG, "Destroying neighbor_table!");
    while (table->head != NULL)
        remove_neighbor_node_from_node(table, table->head);
    ESP_LOGW(NEIGHBOR_TAG, "Destroying neighbor_table finished!");
}