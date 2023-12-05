/*
 * @Author: Zhenwei-Song zhenwei.song@qq.com
 * @Date: 2023-11-13 16:00:10
 * @LastEditors: Zhenwei-Song zhenwei.song@qq.com
 * @LastEditTime: 2023-11-24 15:56:44
 * @FilePath: \esp32\gatt_server_service_table_modified\main\data_manage.c
 * @Description: 仅供学习交流使用
 * Copyright (c) 2023 by Zhenwei-Song, All Rights Reserved.
 */
#include "data_manage.h"
#include "ble_queue.h"
#include "esp_gap_ble_api.h"
#include "neighbor_table.h"
#include "routing_table.h"

SemaphoreHandle_t xCountingSemaphore_send;
SemaphoreHandle_t xCountingSemaphore_receive;

my_info my_information;

uint8_t threshold_high[QUALITY_LEN] = {THRESHOLD_HIGH, 0X00};

uint8_t threshold_low[QUALITY_LEN] = {THRESHOLD_LOW, 0X00};

uint8_t phello_final[PHELLO_FINAL_DATA_LEN] = {PHELLO_FINAL_DATA_LEN - 1, ESP_BLE_AD_TYPE_PHELLO};

uint8_t anhsp_final[ANHSP_FINAL_DATA_LEN] = {ANHSP_FINAL_DATA_LEN - 1, ESP_BLE_AD_TYPE_ANHSP};

uint8_t hsrrep_final[HSRREP_FINAL_DATA_LEN] = {HSRREP_FINAL_DATA_LEN - 1, ESP_BLE_AD_TYPE_HSRREP};

uint8_t anrreq_final[ANRREQ_FINAL_DATA_LEN] = {ANRREQ_FINAL_DATA_LEN - 1, ESP_BLE_AD_TYPE_ANRREQ};

uint8_t anrrep_final[ANRREP_FINAL_DATA_LEN] = {ANRREP_FINAL_DATA_LEN - 1, ESP_BLE_AD_TYPE_ANRREP};

uint8_t temp_quality_of_mine[QUALITY_LEN] = {0};

uint8_t temp_data_31[FINAL_DATA_LEN] = {0};

uint8_t adv_data_final_for_hello[FINAL_DATA_LEN] = {0};
uint8_t adv_data_final_for_anhsp[FINAL_DATA_LEN] = {0};
uint8_t adv_data_final_for_hsrrep[FINAL_DATA_LEN] = {0};
uint8_t adv_data_final_for_anrreq[FINAL_DATA_LEN] = {0};
uint8_t adv_data_final_for_anrrep[FINAL_DATA_LEN] = {0};

uint8_t adv_data_name_7[HEAD_DATA_LEN] = {
    HEAD_DATA_LEN - 1, ESP_BLE_AD_TYPE_NAME_CMPL, 'O', 'L', 'T', 'H', 'R'};

uint8_t adv_data_31[31] = {
    /* device name */
    0x06, 0x09, 'O', 'L', 'T', 'H', 'R',
    /*自定义数据段2*/
    0x17,
    0xff, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16};

uint8_t adv_data_62[62] = {
    /* device name */
    0x06, 0x09, 'O', 'L', 'T', 'H', 'R',
    /*自定义数据段2*/
    0x36,
    0xff, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09};

/**
 * @description:广播数据段拼接
 * @param {uint8_t} *data1  前段数据段
 * @param {uint8_t} *data2  后段数据段
 * @return {*}  返回拼接后的数据
 */
uint8_t *data_match(uint8_t *data1, uint8_t *data2, uint8_t data_1_len, uint8_t data_2_len)
{
    memset(temp_data_31, 0, sizeof(temp_data_31) / sizeof(temp_data_31[0]));
    if (data_1_len + data_2_len <= FINAL_DATA_LEN) {
        memcpy(temp_data_31, data1, data_1_len);
        memcpy(temp_data_31 + data_1_len, data2, data_2_len);
        return temp_data_31;
    }
    else {
        ESP_LOGE(DATA_TAG, "the length of data error");
        return NULL;
    }
}

uint8_t *quality_calculate(int rssi, uint8_t *quality_from_upper, uint8_t distance)
{
    double temp_rssi = (double)(-rssi);
    double temp_quality;
    uint8_t *temp_quality_from_upper = (uint8_t *)malloc(sizeof(uint8_t) * QUALITY_LEN);
    memcpy(temp_quality_from_upper, quality_from_upper, QUALITY_LEN);
    double upper_integer_part = (double)temp_quality_from_upper[0];
    double upper_decimal_part = (double)temp_quality_from_upper[1] / 100;
    double upper_reconstructed_part = upper_integer_part + upper_decimal_part;
    temp_quality = (upper_reconstructed_part + temp_rssi * (distance + 1)) / 10;
    uint8_t my_integer_part = (uint8_t)temp_quality;                             // 得到整数部分
    uint8_t my_decimal_part = (uint8_t)((temp_quality - my_integer_part) * 100); // 得到小数部分
    temp_quality_of_mine[0] = 100 - my_integer_part;
    temp_quality_of_mine[1] = 100 - my_decimal_part;
#if 0
    ESP_LOGE(DATA_TAG, "quality_calculate finished");
    ESP_LOGW(DATA_TAG, "temp_quality_from_upper:");
    esp_log_buffer_hex(DATA_TAG, temp_quality_from_upper, QUALITY_LEN);
    ESP_LOGW(DATA_TAG, "temp_rssi :%f", temp_rssi);
    ESP_LOGW(DATA_TAG, "upper_integer_part :%f", upper_integer_part);
    ESP_LOGW(DATA_TAG, "upper_decimal_part :%f", upper_decimal_part);
    ESP_LOGW(DATA_TAG, "upper_reconstructed_part :%f", upper_reconstructed_part);
    ESP_LOGW(DATA_TAG, "my_integer_part :%d", my_integer_part);
    ESP_LOGW(DATA_TAG, "my_decimal_part :%d", my_decimal_part);
    ESP_LOGW(DATA_TAG, "temp_quality :%f", temp_quality);
    ESP_LOGW(DATA_TAG, "calculate quality:");
    esp_log_buffer_hex(DATA_TAG, temp_quality_of_mine, QUALITY_LEN);
#endif
    free(temp_quality_from_upper);
    return temp_quality_of_mine;
}

/**
 * @description:my_infomation初始化
 * @param {p_my_info} my_information
 * @param {uint8_t} *my_mac
 * @return {*}
 */
void my_info_init(p_my_info my_information, uint8_t *my_mac)
{
    memcpy(my_information->my_id, my_mac + 4, ID_LEN);
#ifdef SELF_ROOT
    my_information->is_root = true;
    my_information->is_connected = true;
    my_information->distance = 0;
    memset(my_information->quality, 0, QUALITY_LEN);
    my_information->update = 0;
    memcpy(my_information->root_id, my_mac + 4, ID_LEN);
    memcpy(my_information->next_id, my_mac + 4, ID_LEN);
#else
    my_information->is_root = false;
    my_information->is_connected = false;
    my_information->distance = 0;
    my_information->quality[0] = NOR_NODE_INIT_QUALITY;
    my_information->update = 0;
#endif
}

/**
 * @description: 生成phello包
 * @param {p_my_info} info
 * @return {*} 返回生成的phello包（带adtype）
 */
uint8_t *generate_phello(p_my_info info)
{
    uint8_t phello[PHELLO_DATA_LEN] = {0};
    uint8_t temp_my_id[ID_LEN];
    uint8_t temp_root_id[ID_LEN];
    uint8_t temp_next_id[ID_LEN];
    uint8_t temp_quality[QUALITY_LEN];
    memcpy(temp_my_id, info->my_id, ID_LEN);
    memcpy(temp_root_id, info->root_id, ID_LEN);
    memcpy(temp_next_id, info->next_id, ID_LEN);
    memcpy(temp_quality, info->quality, QUALITY_LEN);

    if (info->is_root)     // 自己是根节点
        phello[1] |= 0x11; // 节点类型，入网标志
    else                   // 自己不是根节点
        phello[1] |= 0x00; // 节点类型为非root

    phello[1] |= info->is_connected; // 入网标志
    phello[0] |= info->moveable;     // 移动性
    phello[4] |= info->distance;     // 到root跳数
    phello[6] |= temp_quality[0];    // 链路质量
    phello[7] |= temp_quality[1];
    phello[8] |= temp_my_id[0]; // 节点ID
    phello[9] |= temp_my_id[1];
    phello[10] |= temp_root_id[0]; // root ID
    phello[11] |= temp_root_id[1];
    phello[12] |= temp_next_id[0]; // next ID
    phello[13] |= temp_next_id[1];
    phello[15] |= info->update; // 最新包号
    memcpy(phello_final + 2, phello, PHELLO_DATA_LEN);
    return phello_final;
}

/**
 * @description: 解析phello包
 * @param {uint8_t} *phello_data
 * @param {p_my_info} info
 * @return {*}
 */
void resolve_phello(uint8_t *phello_data, p_my_info info, int rssi)
{
    p_phello_info temp_info = (p_phello_info)malloc(sizeof(phello_info));
    uint8_t temp[PHELLO_DATA_LEN];
    memcpy(temp, phello_data, PHELLO_DATA_LEN);
    temp_info->moveable = ((temp[0] & 0x01) == 0x01 ? true : false);
    temp_info->is_root = ((temp[1] & 0x10) == 0x10 ? true : false);
    temp_info->is_connected = ((temp[1] & 0x01) == 0x01 ? true : false);
    temp_info->distance = temp[4];
    // temp_info->quality = temp[7];
    memcpy(temp_info->quality, temp + 6, QUALITY_LEN);
    memcpy(temp_info->node_id, temp + 8, ID_LEN);
    memcpy(temp_info->next_id, temp + 12, ID_LEN);
    temp_info->update = temp[15];
    if (info->is_root == false && temp_info->is_connected == true) {
        memcpy(info->root_id, temp + 10, ID_LEN);
    }

#if 0
    if (temp_info->is_connected) {
        info->distance = temp_info->distance + 1;
        memcpy(temp_quality, quality_calculate(rssi, temp_info->quality, info), QUALITY_LEN);
    }
    else {
        memcpy(temp_quality, temp_info->quality, QUALITY_LEN);
    }
#endif
    /* -------------------------------------------------------------------------- */
    /*                                    更新邻居表                                   */
    /* -------------------------------------------------------------------------- */
    insert_neighbor_node(&my_neighbor_table, temp_info->node_id, temp_info->is_root,
                         temp_info->is_connected, temp_info->quality, temp_info->distance, rssi);
#if 0
    if (info->is_root == true) {                // 若root dead后重回网络
        if (temp_info->update > info->update) { // 自己重新上电
            if (temp_info->update == 255)       // 启动循环
                info->update = 2;
            else
                info->update = temp_info->update + 1;
            ESP_LOGE(ROUTING_TAG, "!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        }
    }
    else {                                                      // 自己非root
        if (temp_info->distance == 100) {                       // 接收到root dead信息
            if (info->distance == 1 || info->distance == 100) { // 自己是根节点的邻居节点，或者已经更新过自己的info
            }
            else {
                info->is_connected |= 0;
                info->distance = 100;
                info->update = temp_info->update;
                memset(info->root_id, 0, ID_LEN);
                memset(info->next_id, 0, ID_LEN);
            }
        }
        else {                                                                 // 不是root dead信息
            if (info->distance > temp_info->distance || info->distance == 0) { // 仅从父节点更新自己状态
                // 自己不是root
                if (temp_info->update > info->update || (info->update - temp_info->update == 253 && temp_info->update == 2)) { // root is back，更新当前信息
                    info->is_connected |= 1;                                                                                   // 自己也已入网
                    info->distance = temp_info->distance + 1;                                                                  // 距离加1
                    info->update = temp_info->update;
                    memcpy(info->root_id, temp_info->root_id, ID_LEN); // 存储root id

                    // memcpy(info->next_id, temp_info->next_id, ID_LEN); // 把发送该hello包的节点作为自己的到root下一跳id
                }
                else if (temp_info->update == info->update) { // root is alive 正常接收
                    if (temp_info->is_connected == 1) {
                        if (info->distance == 0 || info->distance - temp_info->distance > 1)
                            info->distance = temp_info->distance + 1; // 距离加1
                        info->is_connected |= 1;                      // 自己也已入网
                    }
                    else {
                        info->is_connected |= 0;
                    }
                    memcpy(info->root_id, temp_info->root_id, ID_LEN); // 存储root id
                }
                else { // 旧update的包，丢弃
                }
            }
        }
    }
#endif
}

/**
 * @description:在阈值中间区域时，发送的入网请求包
 * @param {p_my_info} info
 * @return {*}
 */
uint8_t *generate_anhsp(p_my_info info)
{
    uint8_t anhsp[ANHSP_DATA_LEN] = {0};
    uint8_t temp_my_id[ID_LEN];
    uint8_t temp_root_id[ID_LEN];
    uint8_t temp_next_id[ID_LEN];
    uint8_t temp_quality[QUALITY_LEN];
    memcpy(temp_my_id, info->my_id, ID_LEN);
    memcpy(temp_root_id, info->root_id, ID_LEN);
    memcpy(temp_next_id, info->next_id, ID_LEN);
    memcpy(temp_quality, info->quality, QUALITY_LEN);

    anhsp[1] |= 0; // 初始距离定为0
    anhsp[2] |= temp_quality[0];
    anhsp[3] |= temp_quality[1];
    anhsp[4] |= temp_my_id[0]; // 节点ID
    anhsp[5] |= temp_my_id[1];
    anhsp[6] |= temp_my_id[0]; // 源 ID
    anhsp[7] |= temp_my_id[1];
    anhsp[8] |= temp_next_id[0]; // next ID
    anhsp[9] |= temp_next_id[1];
    anhsp[10] |= temp_root_id[0]; // 上一任root ID
    anhsp[11] |= temp_root_id[1];
    memcpy(anhsp_final + 2, anhsp, ANHSP_DATA_LEN);
    return anhsp_final;
}

/**
 * @description: 在阈值中间区域时，发送的入网请求包（中转）
 * @param {p_anhsp_info} anhsp_info
 * @param {p_my_info} info
 * @return {*}
 */
uint8_t *generate_transfer_anhsp(p_anhsp_info anhsp_info, p_my_info info)
{
    uint8_t anhsp[ANHSP_DATA_LEN] = {0};
    uint8_t temp_my_id[ID_LEN];
    uint8_t temp_root_id[ID_LEN];
    uint8_t temp_source_id[ID_LEN];
    uint8_t temp_next_id[ID_LEN];
    uint8_t temp_quality[QUALITY_LEN];
    memcpy(temp_my_id, info->my_id, ID_LEN);
    memcpy(temp_root_id, info->root_id, ID_LEN);
    memcpy(temp_source_id, anhsp_info->source_id, ID_LEN);
    memcpy(temp_next_id, info->next_id, ID_LEN);
    memcpy(temp_quality, anhsp_info->quality, QUALITY_LEN);

    anhsp[1] |= anhsp_info->distance + 1; // 经过一次转发，距离加1
    anhsp[2] |= temp_quality[0];
    anhsp[3] |= temp_quality[1];
    anhsp[4] |= temp_my_id[0]; // 节点ID
    anhsp[5] |= temp_my_id[1];
    anhsp[6] |= temp_source_id[0]; // 源 ID
    anhsp[7] |= temp_source_id[1];
    anhsp[8] |= temp_next_id[0]; // next ID
    anhsp[9] |= temp_next_id[1];
    anhsp[10] |= temp_root_id[0]; // 上一任root ID
    anhsp[11] |= temp_root_id[1];
    memcpy(anhsp_final + 2, anhsp, ANHSP_DATA_LEN);
    return anhsp_final;
}

/**
 * @description: 在阈值中间区域时，发送的入网请求包（解析）
 * @param {uint8_t} *anhsp_data
 * @param {p_my_info} info
 * @return {*}
 */
void resolve_anhsp(uint8_t *anhsp_data, p_my_info info)
{
    p_anhsp_info temp_info = (p_anhsp_info)malloc(sizeof(anhsp_info));
    uint8_t temp[ANHSP_DATA_LEN];
    memcpy(temp, anhsp_data, ANHSP_DATA_LEN);
    temp_info->distance = temp[1];
    memcpy(temp_info->quality, temp + 2, QUALITY_LEN);
    memcpy(temp_info->node_id, temp + 4, ID_LEN);
    memcpy(temp_info->source_id, temp + 6, ID_LEN);
    memcpy(temp_info->next_id, temp + 8, ID_LEN);
    memcpy(temp_info->last_root_id, temp + 10, ID_LEN);

    /* -------------------------------------------------------------------------- */
    /*                                  开始转发到root                                 */
    /* -------------------------------------------------------------------------- */
    if (info->is_root) { // 根节点 回复hsrrep
        insert_routing_node(&my_routing_table, info->root_id, temp_info->source_id, temp_info->node_id, temp_info->distance + 1);
        memcpy(adv_data_final_for_hsrrep, data_match(adv_data_name_7, generate_hsrrep(info, temp_info->source_id), HEAD_DATA_LEN, HSRREP_FINAL_DATA_LEN), FINAL_DATA_LEN);
        queue_push(&send_queue, adv_data_final_for_hsrrep, 0);
        xSemaphoreGive(xCountingSemaphore_send);
        ESP_LOGE(DATA_TAG, "response hsrrep");
    }
    else {
        if (info->is_connected && memcmp(temp_info->next_id, info->my_id, ID_LEN) == 0) { // 由自己中转，开始转发
            insert_routing_node(&my_routing_table, info->root_id, temp_info->source_id, temp_info->node_id, temp_info->distance + 1);
            memcpy(adv_data_final_for_anhsp, data_match(adv_data_name_7, generate_transfer_anhsp(temp_info, info), HEAD_DATA_LEN, ANHSP_FINAL_DATA_LEN), FINAL_DATA_LEN);
            queue_push(&send_queue, adv_data_final_for_anhsp, 0);
            xSemaphoreGive(xCountingSemaphore_send);
            ESP_LOGE(DATA_TAG, "transer anhsp");
            temp_info->distance = temp_info->distance + 1;
            /// insert_routing_node(&my_routing_table, temp_info->source_id, temp_info->last_root_id, temp_info->node_id, temp_info->distance); // 记录入路由表，用于反向路由
            print_routing_table(&my_routing_table);
        }
        else { // 不是由自己中转，不处理
            ESP_LOGE(DATA_TAG, "get anhsp,but not transfer");
        }
    }
}

/**
 * @description: 根节点向发送中间区域入网请求包的节点发出的回应包
 * @param {p_anhsp_info} anhsp_info
 * @param {p_my_info} info
 * @return {*}
 */
uint8_t *generate_hsrrep(p_my_info info, uint8_t *des_id)
{
    uint8_t hsrrep[HSRREP_DATA_LEN] = {0};
    uint8_t temp_my_id[ID_LEN];
    uint8_t *temp_next_id;
    uint8_t temp_destination_id[ID_LEN];
    uint8_t temp_reverse_next_id[ID_LEN];
    memcpy(temp_my_id, info->my_id, ID_LEN);
    memcpy(temp_destination_id, des_id, ID_LEN);
    temp_next_id = get_next_id(&my_routing_table, des_id);
    if (temp_next_id != NULL)
        memcpy(temp_reverse_next_id, temp_next_id, ID_LEN);
    hsrrep[1] |= 0;             // distance
    hsrrep[4] |= temp_my_id[0]; // 节点ID
    hsrrep[5] |= temp_my_id[1];
    hsrrep[6] |= temp_destination_id[0]; // 目的 ID
    hsrrep[7] |= temp_destination_id[1];
    hsrrep[8] |= temp_reverse_next_id[0]; // 下一跳id
    hsrrep[9] |= temp_reverse_next_id[1];
    memcpy(hsrrep_final + 2, hsrrep, HSRREP_DATA_LEN);
    return hsrrep_final;
}

/**
 * @description: 根节点向发送中间区域入网请求包的节点发出的回应包（中转）
 * @param {p_hsrrep_info} hsrrep_info
 * @param {p_my_info} info
 * @return {*}
 */
uint8_t *generate_transfer_hsrrep(p_hsrrep_info hsrrep_info, p_my_info info)
{
    uint8_t hsrrep[HSRREP_DATA_LEN] = {0};
    uint8_t temp_my_id[ID_LEN];
    uint8_t temp_destination_id[ID_LEN];
    uint8_t temp_reverse_next_id[ID_LEN];
    memcpy(temp_my_id, info->my_id, ID_LEN);
    memcpy(temp_destination_id, hsrrep_info->destination_id, ID_LEN);
    memcpy(temp_reverse_next_id, get_next_id(&my_routing_table, temp_destination_id), ID_LEN);

    hsrrep[1] = hsrrep_info->distance + 1;
    hsrrep[4] |= temp_my_id[0]; // 节点ID
    hsrrep[5] |= temp_my_id[1];
    hsrrep[6] |= temp_destination_id[0]; // 目的 ID
    hsrrep[7] |= temp_destination_id[1];
    hsrrep[8] |= temp_reverse_next_id[0]; // 下一跳id
    hsrrep[9] |= temp_reverse_next_id[1];

    memcpy(hsrrep_final + 2, hsrrep, HSRREP_DATA_LEN);
    return hsrrep_final;
}

/**
 * @description: 根节点向发送中间区域入网请求包的节点发出的回应包（解析）
 * @param {uint8_t} *hsrrep_data
 * @param {p_my_info} info
 * @return {*}
 */
void resolve_hsrrep(uint8_t *hsrrep_data, p_my_info info)
{
    p_hsrrep_info temp_info = (p_hsrrep_info)malloc(sizeof(hsrrep_info));
    uint8_t temp[ANHSP_DATA_LEN];
    memcpy(temp, hsrrep_data, ANHSP_DATA_LEN);
    memcpy(temp_info->node_id, temp + 4, ID_LEN);
    memcpy(temp_info->destination_id, temp + 6, ID_LEN);
    memcpy(temp_info->reverse_next_id, temp + 8, ID_LEN);
    temp_info->distance = hsrrep_data[1];
    if (memcmp(temp_info->reverse_next_id, info->my_id, ID_LEN) == 0) {
        if (memcmp(temp_info->destination_id, info->my_id, ID_LEN) == 0) { // 收到根节点发给我的hsrrep
            info->is_connected |= true;                                    // TODO:1.未加入等待时间   2.未收到的情况待写
            info->distance = temp_info->distance + 1;
            ESP_LOGE(DATA_TAG, "receive hsrrep");
        }
        else {                                // 不是发给我的hsrrep
            if (info->is_connected == true) { // 由入网的节点转发
                // insert_routing_node(&my_routing_table, info->root_id, temp_info->destination_id, temp_info->node_id, temp_info->distance + 1);
                memcpy(adv_data_final_for_hsrrep, data_match(adv_data_name_7, generate_transfer_hsrrep(temp_info, info), HEAD_DATA_LEN, HSRREP_FINAL_DATA_LEN), FINAL_DATA_LEN);
                queue_push(&send_queue, adv_data_final_for_hsrrep, 0);
                xSemaphoreGive(xCountingSemaphore_send);
                ESP_LOGE(DATA_TAG, "transfer hsrrep");
            }
        }
    }
    else {
        ESP_LOGE(DATA_TAG, "hsrrep next id is not me,do nothing");
    }
}

/**
 * @description: 在阈值下部区域时，发送的入网请求包
 * @param {p_my_info} info
 * @return {*}
 */
uint8_t *generate_anrreq(p_my_info info)
{
    uint8_t anrreq[ANRREQ_DATA_LEN] = {0};
    uint8_t temp_threshold[THRESHOLD_LEN];
    uint8_t temp_source_id[ID_LEN];
    uint8_t temp_serial_number[SERIAL_NUM_LEN];
    memcpy(temp_threshold, info->threshold, THRESHOLD_LEN);
    memcpy(temp_source_id, info->my_id, ID_LEN);
    // memcpy(temp_destination_id, anhsp_info->destination_id, ID_LEN);
    // TODO:序列号
    anrreq[0] |= temp_threshold[0]; // 链路质量阈值
    anrreq[1] |= temp_threshold[1];
    anrreq[2] |= temp_source_id[0]; // 源 ID
    anrreq[3] |= temp_source_id[1];

    memcpy(anrreq_final + 2, anrreq, ANRREQ_DATA_LEN);
    return anrreq_final;
}

/**
 * @description: 在阈值下部区域时，发送的入网请求包（解析）
 * @param {uint8_t} *anrreq_data
 * @param {p_my_info} info
 * @return {*}
 */
void resolve_anrreq(uint8_t *anrreq_data, p_my_info info)
{
    p_anrreq_info temp_info = (p_anrreq_info)malloc(sizeof(anrreq_info));
    uint8_t temp[ANRREQ_DATA_LEN];
    memcpy(temp, anrreq_data, ANRREQ_DATA_LEN);
    memcpy(temp_info->quality_threshold, temp, THRESHOLD_LEN);
    memcpy(temp_info->source_id, temp + 2, ID_LEN);
    // TODO:序列号
}