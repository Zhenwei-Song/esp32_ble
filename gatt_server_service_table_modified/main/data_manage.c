/*
 * @Author: Zhenwei-Song zhenwei.song@qq.com
 * @Date: 2023-11-13 16:00:10
 * @LastEditors: Zhenwei-Song zhenwei.song@qq.com
 * @LastEditTime: 2023-11-17 09:51:34
 * @FilePath: \esp32\gatt_server_service_table_modified\main\data_manage.c
 * @Description: 仅供学习交流使用
 * Copyright (c) 2023 by Zhenwei-Song, All Rights Reserved.
 */
#include "data_manage.h"
#include "routing_table.h"

my_info my_information;

uint8_t phello_final[PHELLO_FINAL_DATA_LEN] = {0x11, 0xf1};

uint8_t temp_data_31[FINAL_DATA_LEN] = {0};

uint8_t adv_data_final[FINAL_DATA_LEN] = {0};

uint8_t adv_data_name_7[HEAD_DATA_LEN] = {
    0x06, 0x09, 'O', 'L', 'T', 'H', 'R'};

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

/*
mask:
0X01:0000 0001
0X02:0000 0010
0X04:0000 0100
0X08:0000 1000
*/

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
    my_information->update = 0;
    memcpy(my_information->root_id, my_mac + 4, ID_LEN);
    memcpy(my_information->next_id, my_mac + 4, ID_LEN);
#else
    my_information->is_root = false;
    my_information->is_connected = false;
    my_information->distance = 0;
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
    memcpy(temp_my_id, info->my_id, ID_LEN);
    memcpy(temp_root_id, info->root_id, ID_LEN);
    memcpy(temp_next_id, info->next_id, ID_LEN);

    if (info->is_root)     // 自己是根节点
        phello[1] |= 0x11; // 节点类型，入网标志
    else                   // 自己不是根节点
        phello[1] |= 0x00; // 节点类型为非root

    phello[1] |= info->is_connected; // 入网标志
    phello[0] |= info->moveable;     // 移动性
    phello[4] |= info->distance;     // 到root跳数
    phello[8] |= temp_my_id[0];      // 节点ID
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
void resolve_phello(uint8_t *phello_data, p_my_info info)
{
    p_phello_info temp_info = (p_phello_info)malloc(sizeof(phello_info));
    uint8_t temp[PHELLO_DATA_LEN];
    memcpy(temp, phello_data, PHELLO_DATA_LEN);
    temp_info->moveable = ((temp[0] & 0x01) == 0x01 ? true : false);
    temp_info->is_root = ((temp[1] & 0x10) == 0x10 ? true : false);
    temp_info->is_connected = ((temp[1] & 0x01) == 0x01 ? true : false);
    temp_info->distance = temp[4];
    memcpy(temp_info->quality, temp + 6, QUALITY_LEN);
    memcpy(temp_info->node_id, temp + 8, ID_LEN);
    memcpy(temp_info->root_id, temp + 10, ID_LEN);
    memcpy(temp_info->next_id, temp + 12, ID_LEN);
    temp_info->update = temp[15];

    /* -------------------------------------------------------------------------- */
    /*                                    更新邻居表                                   */
    /* -------------------------------------------------------------------------- */
    insert_routing_node(&neighbor_table, temp_info->node_id, temp_info->is_root,
                        temp_info->is_connected, temp_info->quality, temp_info->distance);

    if (info->is_root == true) { // 若root dead后重回网络
        if (info->update < temp_info->update) {
            if (temp_info->update == 255) // 启动循环
                info->update = 2;
            else
                info->update = temp_info->update + 1;
        }
    }
    else {                                                                                                             // 自己不是root
        if (temp_info->update > info->update || (info->update - temp_info->update == 253 && temp_info->update == 2)) { // 获得最新消息包，更新当前信息
            if (temp_info->is_connected == false) {                                                                    // root is dead
                info->is_connected |= 0;
                info->distance = 0;
            }
            else if (temp_info->is_connected == true) {   // root is back
                info->is_connected |= 1;                  // 自己也已入网
                info->distance = temp_info->distance + 1; // 距离加1
            }
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