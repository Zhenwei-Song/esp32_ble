/*
 * @Author: Zhenwei-Song zhenwei.song@qq.com
 * @Date: 2023-11-11 11:06:54
 * @LastEditors: Zhenwei-Song zhenwei.song@qq.com
 * @LastEditTime: 2023-11-16 11:20:13
 * @FilePath: \esp32\gatt_server_service_table_modified\main\data_manage.h
 * @Description: 仅供学习交流使用
 * Copyright (c) 2023 by Zhenwei-Song, All Rights Reserved.
 */
#ifndef _DATA_H_
#define _DATA_H_
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"

// #define SELF_ROOT

#define DATA_TAG "DATA"

#define ID_LEN 2
#define QUALITY_LEN 2

#define HEAD_DATA_LEN 7
#define FINAL_DATA_LEN 31
#define PHELLO_FINAL_DATA_LEN 18
#define PHELLO_DATA_LEN PHELLO_FINAL_DATA_LEN - 2

typedef struct my_info {
    bool moveable;
    bool is_root;
    bool ready_to_connect;
    bool is_connected;
    int8_t distance;
    uint8_t my_id[ID_LEN];
    uint8_t root_id[ID_LEN];
    uint8_t next_id[ID_LEN];
} my_info, *p_my_info;

typedef struct phello_info {
    bool moveable;
    bool is_root;
    bool is_connected;
    uint8_t distance;
    uint8_t quality[QUALITY_LEN];
    uint8_t node_id[ID_LEN];
    uint8_t root_id[ID_LEN];
    uint8_t next_id[ID_LEN];
} phello_info, *p_phello_info;

extern my_info my_information;

extern uint8_t temp_id[ID_LEN];

extern uint8_t phello_final[PHELLO_FINAL_DATA_LEN];

extern uint8_t adv_data_name_7[HEAD_DATA_LEN];

extern uint8_t temp_data_31[FINAL_DATA_LEN];

extern uint8_t adv_data_final[FINAL_DATA_LEN];

// 31字节数据
extern uint8_t adv_data_31[31];

extern uint8_t adv_data_62[62];

uint8_t *data_match(uint8_t *data1, uint8_t *data2, uint8_t data_1_len, uint8_t data_2_len);

void my_info_init(p_my_info my_information, uint8_t *my_mac);

uint8_t *generate_phello(p_my_info info);

void resolve_phello(uint8_t *phello_data, p_my_info info);

#if 0
uint8_t *generate_main_quest(p_my_info info);

void resolve_main_quest(uint8_t *phello_data, p_my_info info);

uint8_t *generate_main_answer(p_my_info info);

void resolve_main_answer(uint8_t *phello_data, p_my_info info);

uint8_t *generate_second_quest(p_my_info info);

void resolve_second_quest(uint8_t *phello_data, p_my_info info);

uint8_t *generate_second_answer(p_my_info info);

void resolve_second_answer(uint8_t *phello_data, p_my_info info);

uint8_t *generate_repair(p_my_info info);

void resolve_repair(uint8_t *phello_data, p_my_info info);

#endif

#endif // _DATA_H_