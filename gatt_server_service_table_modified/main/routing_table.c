/*
 * @Author: Zhenwei-Song zhenwei.song@qq.com
 * @Date: 2023-11-09 15:05:15
 * @LastEditors: Zhenwei-Song zhenwei.song@qq.com
 * @LastEditTime: 2023-11-11 09:45:31
 * @FilePath: \esp32\gatt_server_service_table_modified\main\routing_table.c
 * @Description: 仅供学习交流使用
 * Copyright (c) 2023 by Zhenwei-Song, All Rights Reserved.
 */
/*
 * @Author: Zhenwei-Song zhenwei.song@qq.com
 * @Date: 2023-11-09 15:05:15
 * @LastEditors: Zhenwei-Song zhenwei.song@qq.com
 * @LastEditTime: 2023-11-10 21:26:31
 * @FilePath: \esp32\gatt_server_service_table_modified\main\routing_table.c
 * @Description: 仅供学习交流使用
 * Copyright (c) 2023 by Zhenwei-Song, All Rights Reserved.
 */

#include "esp_log.h"

#include "routing_table.h"

/**
 * @description: 初始化路由表
 * @param {p_routing_table} table
 * @return {*}
 */
void init_routing_table(p_routing_table table)
{
    table->head = NULL;
}

#if 0
/**
 * @description: 插入邻居链表
 * @param {p_routing_note} new_routing
 * @return {*}
 */
int insert_routing_node(p_routing_table table, p_routing_note new_routing)
{
    p_routing_note cur = table->head;
    int repeated = -1;

    if (table->head == NULL) { // 路由表为空
        table->head = table->tail = new_routing;
        new_routing->next = NULL;
    }
    else {
        while (cur != NULL) {
            for (int i = 0; i < 6; i++) {
                ESP_LOGI(ROUTING_TAG, "cur->mac: %x", cur->mac[i]);
            }
            ESP_LOGI(ROUTING_TAG, "table->head->mac addr: %p", cur->mac);

            repeated = memcmp(cur->mac, new_routing->mac, 6);
            if (repeated == 0) { // 检查重复
                ESP_LOGI(ROUTING_TAG, "repeated mac address found");
                free(new_routing);
                goto exit;
            }
            if (cur->next == NULL) { // 链表中的最后一个节点
                break;
            }
            cur = cur->next;
        }
        table->tail->next = new_routing; // 表尾添加新项
        table->tail = new_routing;
        new_routing->next = NULL;
    }
    ESP_LOGW(ROUTING_TAG, "ADD NEW NOTE");
exit:
    return 0;
}
#else
int insert_routing_node(p_routing_table table, uint8_t *new_mac)
{
    p_routing_note new_node = (p_routing_note)malloc(sizeof(routing_note));
    memcpy(new_node->mac, new_mac, 6);
    p_routing_note cur = table->head;
    int repeated = -1;

    if (table->head == NULL) { // 路由表为空
        table->head = new_node;
        new_node->next = NULL;
    }
    else {
        while (cur != NULL) {
            // ESP_LOGI(ROUTING_TAG, "table->head->mac addr: %p", cur->mac);
            repeated = memcmp(cur->mac, new_node->mac, 6);
            if (repeated == 0) { // 检查重复
                ESP_LOGI(ROUTING_TAG, "repeated mac address found");
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
#endif
/**
 * @description:从路由链表移除项
 * @param {p_routing_table} table
 * @param {p_routing_note} old_routing
 * @return {*}
 */
void remove_routing_node(p_routing_table table, p_routing_note old_routing)
{
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
bool check_routing_table(p_routing_table table, p_routing_note routing_note)
{
    p_routing_note temp = table->head;
    if (temp == NULL) {
        return false;
    }
    else {
        while (temp != NULL) {
            if (routing_note->mac == temp->mac)
                return true;
            else
                temp = temp->next;
        }
    }
    return false;
}

/**
 * @description: 打印邻居表
 * @param {p_routing_table} table
 * @return {*}
 */
void print_routing_table(p_routing_table table)
{
    p_routing_note temp = table->head;
    ESP_LOGW(ROUTING_TAG, "********************************Start printing MAC address:******************************************************");
    while (temp != NULL) {
        esp_log_buffer_hex(ROUTING_TAG, temp->mac, 6);
        temp = temp->next;
    }
    ESP_LOGW(ROUTING_TAG, "********************************Printing MAC address  is finished ***********************************************");
}

void destroy_routing_table(p_routing_table table)
{
    ESP_LOGW(ROUTING_TAG, "Destroying routing_table!");
    while (table->head != NULL)
        remove_routing_node(table, table->head);
    ESP_LOGW(ROUTING_TAG, "Destroying routing_table finished!");
}