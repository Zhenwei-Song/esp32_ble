/*
 * @Author: Zhenwei-Song zhenwei.song@qq.com
 * @Date: 2023-11-09 15:05:02
 * @LastEditors: Zhenwei-Song zhenwei.song@qq.com
 * @LastEditTime: 2023-11-20 10:56:21
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
    uint8_t quality_from_me[QUALITY_LEN];
    // uint8_t quality;
    uint8_t distance;
    int rssi;
    uint8_t count;
    struct routing_note *next;
} routing_note, *p_routing_note;

typedef struct routing_table {
    p_routing_note head;
} routing_table, *p_routing_table;

typedef struct connected_node {
    uint8_t id[ID_LEN];
    uint8_t quality[QUALITY_LEN];
    uint8_t distance;
} connected_node, *p_connected_node;

extern routing_table neighbor_table;

extern bool refresh_flag;

void init_routing_table(p_routing_table table);

// int insert_routing_node(p_routing_table table, p_routing_note new_routing);
int insert_routing_node(p_routing_table table, uint8_t *new_id, bool is_root, bool is_connected, uint8_t *quality, uint8_t distance, int rssi);

void remove_routing_node_from_node(p_routing_table table, p_routing_note old_routing);

void remove_routing_node(p_routing_table table, uint8_t *old_id);

bool is_routing_table_empty(p_routing_table table);

bool routing_table_check_id(p_routing_table table, uint8_t *id);

//void update_my_connection_info(p_routing_table table, p_my_info my_info);

int set_routing_node_distance(p_routing_table table, uint8_t *id, int8_t distance);

uint8_t get_routing_node_distance(p_routing_table table, uint8_t *id);

void refresh_cnt_routing_table(p_routing_table table, p_my_info info);

void update_quality_of_routing_table(p_routing_table table);

void set_my_next_id_quality_and_distance(p_routing_table table, p_my_info info);

void print_routing_table(p_routing_table table);

void destroy_routing_table(p_routing_table table);

#endif // _ROUTING_TABLE_H_