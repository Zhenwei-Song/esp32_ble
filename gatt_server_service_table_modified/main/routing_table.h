/*
 * @Author: Zhenwei-Song zhenwei.song@qq.com
 * @Date: 2023-11-09 15:05:02
 * @LastEditors: Zhenwei-Song zhenwei.song@qq.com
 * @LastEditTime: 2023-11-16 10:55:09
 * @FilePath: \esp32\gatt_server_service_table_modified\main\routing_table.h
 * @Description: 仅供学习交流使用
 * Copyright (c) 2023 by Zhenwei-Song, All Rights Reserved.
 */
#ifndef _ROUTING_TABLE_H_
#define _ROUTING_TABLE_H_

#define ROUTING_TAG "ROUTING_TABLE"

#include "data_manage.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROUTING_TABLE_COUNT 10

typedef struct routing_note {
    uint8_t id[ID_LEN];
    bool is_root;
    bool is_connected;
    uint8_t quality[QUALITY_LEN];
    uint8_t distance;
    uint8_t count;
    struct routing_note *next;
} routing_note, *p_routing_note;

typedef struct routing_table {
    p_routing_note head;
} routing_table, *p_routing_table;

extern routing_table neighbor_table;

void init_routing_table(p_routing_table table);

// int insert_routing_node(p_routing_table table, p_routing_note new_routing);
int insert_routing_node(p_routing_table table, uint8_t *new_id, bool is_root, bool is_connected, uint8_t *quality, uint8_t distance);

void remove_routing_node_from_node(p_routing_table table, p_routing_note old_routing);

void remove_routing_node(p_routing_table table, uint8_t *old_id);

bool is_routing_table_empty(p_routing_table table);

bool routing_table_check_id(p_routing_table table, uint8_t *id);

int set_routing_node_distance(p_routing_table table, uint8_t *id, int8_t distance);

int8_t get_routing_node_distance(p_routing_table table, uint8_t *id);

void refresh_cnt_routing_table(p_routing_table table, p_my_info info);

void print_routing_table(p_routing_table table);

void destroy_routing_table(p_routing_table table);

#endif // _ROUTING_TABLE_H_