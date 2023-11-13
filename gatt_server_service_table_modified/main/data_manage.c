/*
 * @Author: Zhenwei-Song zhenwei.song@qq.com
 * @Date: 2023-11-13 16:00:10
 * @LastEditors: Zhenwei-Song zhenwei.song@qq.com
 * @LastEditTime: 2023-11-13 16:02:35
 * @FilePath: \esp32\gatt_server_service_table_modified\main\data_manage.c
 * @Description: 仅供学习交流使用
 * Copyright (c) 2023 by Zhenwei-Song, All Rights Reserved.
 */
#include "data_manage.h"

/**
 * @description:广播数据段拼接
 * @param {uint8_t} *data1  前段数据段
 * @param {uint8_t} *data2  后段数据段
 * @return {*}  无返回值
 */
void data_match(uint8_t *data1, uint8_t *data2)
{
    data_1_size = strlen((char *)data1);
    data_2_size = strlen((char *)data2);
    uint8_t data_final_size = sizeof(adv_data_final);
    if (data_1_size + data_2_size <= data_final_size) {
        for (int i = 0; i < data_1_size; i++) {
            adv_data_final[i] = data1[i];
        }
        for (int i = 0; i < data_2_size; i++) {
            adv_data_final[i + data_1_size] = data2[i];
        }
    }
    else {
        ESP_LOGE(DATA_TAG, "the length of data error");
    }
}