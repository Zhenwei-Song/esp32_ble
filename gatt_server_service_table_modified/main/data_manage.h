/*
 * @Author: Zhenwei-Song zhenwei.song@qq.com
 * @Date: 2023-11-11 11:06:54
 * @LastEditors: Zhenwei Song zhenwei.song@qq.com
 * @LastEditTime: 2024-01-16 19:50:06
 * @FilePath: \esp32\esp32_ble\gatt_server_service_table_modified\main\data_manage.h
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

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_log.h"

// #define SELF_ROOT

#define DATA_TAG "DATA"

#define NOR_NODE_INIT_QUALITY 0
#define NOR_NODE_INIT_DISTANCE 100

#define WAIT_TIME_1 5
#define WAIT_TIME_2 10

#define ID_LEN 2
#define QUALITY_LEN 2
#define THRESHOLD_LEN 2
#define SERIAL_NUM_LEN 2

#define HEAD_DATA_LEN 7
#define FINAL_DATA_LEN 31

#define THRESHOLD_HIGH 100
#define THRESHOLD_LOW 90

#define PHELLO_FINAL_DATA_LEN 18
#define PHELLO_DATA_LEN PHELLO_FINAL_DATA_LEN - 2

#define ANHSP_FINAL_DATA_LEN 14
#define ANHSP_DATA_LEN ANHSP_FINAL_DATA_LEN - 2

#define HSRREP_FINAL_DATA_LEN 14
#define HSRREP_DATA_LEN HSRREP_FINAL_DATA_LEN - 2

#define ANRREQ_FINAL_DATA_LEN 10
#define ANRREQ_DATA_LEN ANRREQ_FINAL_DATA_LEN - 2

#define ANRREP_FINAL_DATA_LEN 14
#define ANRREP_DATA_LEN ANRREP_FINAL_DATA_LEN - 2

extern SemaphoreHandle_t xCountingSemaphore_send;
extern SemaphoreHandle_t xCountingSemaphore_receive;

typedef struct my_info {
    bool moveable;
    bool is_root;
    bool ready_to_connect;
    bool is_connected;
    uint8_t distance;
    uint8_t quality[QUALITY_LEN];
    uint8_t my_id[ID_LEN];
    uint8_t root_id[ID_LEN];
    uint8_t next_id[ID_LEN];
    uint8_t threshold[THRESHOLD_LEN];
    uint8_t update;
} my_info, *p_my_info;

typedef struct phello_info {
    bool moveable;
    bool is_root;
    bool is_connected;
    uint8_t distance;
    // uint8_t quality[QUALITY_LEN];
    uint8_t quality[QUALITY_LEN];
    uint8_t node_id[ID_LEN];
    uint8_t root_id[ID_LEN];
    uint8_t next_id[ID_LEN];
    uint8_t update;

} phello_info, *p_phello_info;

typedef struct anhsp_info {
    uint8_t distance;
    uint8_t quality[QUALITY_LEN];
    uint8_t node_id[ID_LEN];
    uint8_t source_id[ID_LEN];
    uint8_t next_id[ID_LEN];
    uint8_t last_root_id[ID_LEN];
} anhsp_info, *p_anhsp_info;

typedef struct hsrrep_info {
    uint8_t distance;
    uint8_t node_id[ID_LEN];
    uint8_t destination_id[ID_LEN];
    uint8_t reverse_next_id[ID_LEN];
} hsrrep_info, *p_hsrrep_info;

typedef struct anrreq_info {
    uint8_t quality_threshold[THRESHOLD_LEN];
    uint8_t source_id[ID_LEN];
    uint8_t serial_number[SERIAL_NUM_LEN];
} anrreq_info, *p_anrreq_info;

typedef struct anrrep_info {
    uint8_t distance;
    uint8_t quality[QUALITY_LEN];
    uint8_t node_id[ID_LEN];
    uint8_t destination_id[ID_LEN];
    uint8_t root_id[ID_LEN];
    uint8_t serial_number[SERIAL_NUM_LEN];
} anrrep_info, *p_anrrep_info;

extern my_info my_information;

extern uint8_t threshold_high[QUALITY_LEN];

extern uint8_t threshold_low[QUALITY_LEN];

// extern uint8_t temp_id[ID_LEN];

// extern uint8_t phello_final[PHELLO_FINAL_DATA_LEN];

extern uint8_t adv_data_name_7[HEAD_DATA_LEN];

extern uint8_t temp_data_31[FINAL_DATA_LEN];

extern uint8_t adv_data_final_for_hello[FINAL_DATA_LEN];

extern uint8_t adv_data_final_for_anhsp[FINAL_DATA_LEN];

extern uint8_t adv_data_final_for_anrreq[FINAL_DATA_LEN];
// 31字节数据
extern uint8_t adv_data_31[31];

extern uint8_t adv_data_62[62];

uint8_t *data_match(uint8_t *data1, uint8_t *data2, uint8_t data_1_len, uint8_t data_2_len);

uint8_t *quality_calculate(int rssi, uint8_t *quality_from_upper, uint8_t distance);

void my_info_init(p_my_info my_information, uint8_t *my_mac);

uint8_t *generate_phello(p_my_info info);

void resolve_phello(uint8_t *phello_data, p_my_info info, int rssi);

uint8_t *generate_anhsp(p_my_info info);

uint8_t *generate_transfer_anhsp(p_anhsp_info anhsp_info, p_my_info info);

void resolve_anhsp(uint8_t *anhsp_data, p_my_info info);

uint8_t *generate_hsrrep(p_my_info info, uint8_t *des_id);

uint8_t *generate_transfer_hsrrep(p_hsrrep_info hsrrep_info, p_my_info info);

void resolve_hsrrep(uint8_t *hsrrep_data, p_my_info info);

uint8_t *generate_anrreq(p_my_info info);

void resolve_anrreq(uint8_t *anrreq_data, p_my_info info);

uint8_t *generate_anrrep(p_my_info info, uint8_t *des_id);

void resolve_anrrep(uint8_t *anrrep_data, p_my_info info);
#endif // _DATA_H_