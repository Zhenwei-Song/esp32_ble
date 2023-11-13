/*
 * @Author: Zhenwei Song zhenwei.song@qq.com
 * @Date: 2023-09-22 17:13:32
 * @LastEditors: Zhenwei-Song zhenwei.song@qq.com
 * @LastEditTime: 2023-11-13 09:50:18
 * @FilePath: \esp32\gatt_server_service_table_modified\main\gatts_table_creat_demo.c
 * @Description:
 * 该代码用于接收测试（循环发送01到0f的包）
 * 实现了广播与扫描同时进行（基于gap层）
 * 添加了GPIO的测试内容（由宏定义控制是否启动）
 * 实现了基于adtype中name的判断接收包的方法
 * 实现了字符串的拼接（帧头 + 实际内容）
 * 添加了吞吐量测试
 * 添加了rssi
 * Copyright (c) 2023 by Zhenwei Song, All Rights Reserved.
 */

// #define GPIO //GPIO相关
// #define THROUGHPUT//吞吐量测试
#define ROUTINGTABLE // 路由表（链表）
// #define QUEUE        // 发送、接收队列
#define BUTTON // 按键

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GPIO
#include "driver/gpio.h"
#endif // GPIO
#ifdef QUEUE
#include "ble_queue.h"
#endif // QUEUE
#ifdef ROUTINGTABLE
#include "routing_table.h"
#endif // ROUTINGTABLE
#ifdef BUTTON
#include "board.h"
#endif
#include "data.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#define TAG "BLE_BROADCAST"

#define ADV_INTERVAL_MS 2000 // 广播间隔时间，单位：毫秒
#define SCAN_INTERVAL_MS 225
#define DATA_INTERVAL_MS 600

#ifdef GPIO
#define GPIO_OUTPUT_IO_0 18
#define GPIO_OUTPUT_IO_1 19
#define GPIO_OUTPUT_PIN_SEL ((1ULL << GPIO_OUTPUT_IO_0) | (1ULL << GPIO_OUTPUT_IO_1))
#endif // GPIO

#ifdef THROUGHPUT
#define SECOND_TO_USECOND 1000000
static bool start = false;
static uint64_t start_time = 0;
static uint64_t current_time = 0;
static uint64_t cal_len = 0;
#endif // THROUGHPUT

#ifdef ROUTINGTABLE
routing_table my_routing_table;
#endif // ROUTINGTABLE
#ifdef QUEUE
static queue rec_queue;
static queue send_queue;
#endif // QUEUE

uint32_t duration = 0;

// static bool adv_data_changed = false;

static const char remote_device_name[] = "OLTHR";

// 配置广播参数
esp_ble_adv_params_t adv_params = {
#if 1
    .adv_int_min = ADV_INTERVAL_MS / 0.625,        // 广播间隔时间，单位：0.625毫秒
    .adv_int_max = (ADV_INTERVAL_MS + 10) / 0.625, // 广播间隔时间，单位：0.625毫秒
#else
    .adv_int_min = 0x20,
    .adv_int_max = 0x30,
#endif
    .adv_type = ADV_TYPE_NONCONN_IND, // 不可连接
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL, // 在所有三个信道上广播
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_WLST_CON_WLST,
};

// 配置扫描参数
static esp_ble_scan_params_t ble_scan_params = {
    .scan_type = BLE_SCAN_TYPE_PASSIVE,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
#if 1
    .scan_interval = 0xff, // 50 * 0.625ms = 31.25ms
    .scan_window = 0xff,
#else
    .scan_interval = SCAN_INTERVAL_MS / 0.625,
    .scan_window = SCAN_INTERVAL_MS / 0.625,
#endif
    .scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE};

SemaphoreHandle_t xMutex1;
// SemaphoreHandle_t xMutex2;

/**
 * @description:广播数据段拼接
 * @param {uint8_t} *data1  前段数据段
 * @param {uint8_t} *data2  后段数据段
 * @return {*}  无返回值
 */
static void data_match(uint8_t *data1, uint8_t *data2)
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
        ESP_LOGE(TAG, "the length of data error");
    }
}

/**
 * @description: 广播数据切换任务，最后一个字节从0x01变换到0x0f,重复循环
 * @param {void} *pvParameters
 * @return {*}
 */
static void switch_adv_data_task(void *pvParameters)
{
    char i = 0;
    while (1) {
        if (xSemaphoreTake(xMutex1, portMAX_DELAY) == pdTRUE) {
            // adv_data_change();
            adv_data_ff1[8] = i;
            i++;
            data_match(adv_data_name, adv_data_ff1);
            if (i == 16) {
                i = 1;
            }
            esp_ble_gap_config_adv_data_raw(adv_data_final, data_1_size + data_2_size);
        }
        xSemaphoreGive(xMutex1); // 释放互斥信号量
        // uint32_t random_delay = (rand() % (MAX_DELAY_MS - MIN_DELAY_MS + 1)) + MIN_DELAY_MS;
        vTaskDelay(pdMS_TO_TICKS(DATA_INTERVAL_MS));
    }
}

#ifdef QUEUE
static void ble_rec_data_task(void *pvParameters)
{
    uint8_t *user_data = NULL;
    uint8_t *user_data2 = NULL;
    uint8_t *rec_data = NULL;
    uint8_t user_data_len = 0;
    uint8_t user_data2_len = 0;
    while (1) {
        if (!queue_is_empty(&rec_queue)) {
            ESP_LOGI(TAG, "queue is not empty");

#if 1
            rec_data = queue_pop(&rec_queue);
            esp_log_buffer_hex(ROUTING_TAG, rec_data, 31);
#else
            user_data = esp_ble_resolve_adv_data(queue_pop(&rec_queue),
                                                 ESP_BLE_AD_TYPE_USER_1, &user_data_len); // 解析用户数据1
            user_data2 = esp_ble_resolve_adv_data(queue_pop(&rec_queue),
                                                  ESP_BLE_AD_TYPE_USER_2, &user_data2_len); // 解析用户数据2
            ESP_LOGI(TAG, "ADV_DATA:");
            esp_log_buffer_hex(TAG, queue_pop(&rec_queue), 31);
            ESP_LOGI(TAG, "USER_DATA:");
            esp_log_buffer_hex(TAG, user_data, user_data_len);
            ESP_LOGI(TAG, "USER_DATA2:");
            esp_log_buffer_hex("", user_data2, user_data2_len);
            vTaskDelay(pdMS_TO_TICKS(10));
#endif
        }
    }
}

static void ble_send_data_task(void *pvParameters)
{
    while (1) {
        if (!queue_is_empty(&send_queue)) {
            esp_ble_gap_config_adv_data_raw(queue_pop(&send_queue), 31);
            esp_ble_gap_start_advertising(&adv_params);
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}
#endif // QUEUE

#ifdef THROUGHPUT
/**
 * @description: 计算2秒内吞吐量
 * @param {void} *param
 * @return {*}
 */
static void throughput_task(void *param)
{
    while (1) {
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        uint32_t bit_rate = 0;
        if (start_time) {
            current_time = esp_timer_get_time();
            bit_rate = cal_len * SECOND_TO_USECOND / (current_time - start_time);
            ESP_LOGI(TAG, " Bit rate = %" PRIu32 " Byte/s, = %" PRIu32 " bit/s, time = %ds",
                     bit_rate, bit_rate << 3, (int)((current_time - start_time) / SECOND_TO_USECOND));
        }
        else {
            ESP_LOGI(TAG, " Bit rate = 0 Byte/s, = 0 bit/s");
        }
    }
}
#endif // THROUGHPUT

#if 0
static void ble_scan_task(void *pvParameters)
{
    while (1) {
        if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {

            xSemaphoreGive(xMutex); // 释放互斥信号量
            vTaskDelay(pdMS_TO_TICKS(ADV_INTERVAL_MS));
        }
        //        vTaskDelay(pdMS_TO_TICKS(ADV_INTERVAL_MS)); // 等待一段时间后再次扫描
        // ESP_LOGI(TAG, "scanning");
    }
}
#endif

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    uint8_t *adv_name = NULL;
    uint8_t adv_name_len = 0;
    uint8_t *user_data = NULL;
    uint8_t *user_data2 = NULL;
    uint8_t user_data_len = 0;
    uint8_t user_data2_len = 0;
#ifdef THROUGHPUT
    uint32_t bit_rate = 0;
#endif // THROUGHPUT
#ifdef GPIO
    static bool flag = false;
#endif // GPIO
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT: /*!< When raw advertising data set complete, the event comes */
        // ESP_LOGW(TAG, "ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT");
        // esp_ble_gap_stop_scanning();
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        /* advertising start complete event to indicate advertising start successfully or failed */
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Advertising start failed");
        }
        else {
            // esp_ble_gap_start_scanning(duration);
            ESP_LOGI(TAG, "Advertising start successfully");
#ifdef GPIO
            gpio_set_level(GPIO_OUTPUT_IO_0, 0);
            esp_ble_gap_stop_advertising();
#endif
        }
        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Advertising stop failed");
        }
        else {
            ESP_LOGI(TAG, "Stop adv successfully");
#ifdef GPIO
            gpio_set_level(GPIO_OUTPUT_IO_0, 1);
            esp_ble_gap_start_advertising(&adv_params);
#endif
        }
        break;

    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        // scan start complete event to indicate scan start successfully or failed
        if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Scan start failed");
        }
        else {
            ESP_LOGI(TAG, "Scan start successfully");
        }
        break;

    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Scan stop failed");
        }
        else {
            // esp_ble_gap_start_advertising(&adv_params);
            ESP_LOGI(TAG, "Stop scan successfully\n");
        }
        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT: { // 表示已经扫描到BLE设备的广播数据
        esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
        switch (scan_result->scan_rst.search_evt) { // 用于检查扫描事件类型
        case ESP_GAP_SEARCH_INQ_RES_EVT:            // 表示扫描到一个设备的广播数据
            adv_name = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv,
                                                ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len); // 解析广播设备名
            if (adv_name != NULL) {
                if (strlen(remote_device_name) == adv_name_len && strncmp((char *)adv_name, remote_device_name, adv_name_len) == 0) { // 检查扫描到的设备名称是否与指定的 remote_device_name 匹配
#ifdef THROUGHPUT
#if 0
                    current_time = esp_timer_get_time();
                    bit_rate = 31 * SECOND_TO_USECOND / (current_time - start_time);
                    ESP_LOGI(TAG, " Bit rate = %" PRIu32 " Byte/s, = %" PRIu32 " bit/s, time = %ds",
                             bit_rate, bit_rate << 3, (int)((current_time - start_time) / SECOND_TO_USECOND));

                    start_time = esp_timer_get_time();
#else
                    cal_len += 31;
                    if (start == false) {
                        start_time = esp_timer_get_time();
                        start = true;
                    }
#endif
#endif // THROUGHPUT

#ifndef THROUGHPUT
                    ESP_LOGW(TAG, "%s is found", remote_device_name);
#ifdef QUEUE
                    queue_push(&rec_queue, scan_result->scan_rst.ble_adv);
                    queue_print(&rec_queue);
#else
                    user_data = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv,
                                                         ESP_BLE_AD_TYPE_USER_1, &user_data_len); // 解析用户数据1
                    user_data2 = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv,
                                                          ESP_BLE_AD_TYPE_USER_2, &user_data2_len); // 解析用户数据2

                    ESP_LOGI(TAG, "MAC:");
                    esp_log_buffer_hex(TAG, scan_result->scan_rst.bda, 6); // 打印扫描到设备的MAC地址
                                                                           // 打印广播数据
                    ESP_LOGI(TAG, "ADV_DATA:");
                    esp_log_buffer_hex(TAG, param->scan_rst.ble_adv, param->scan_rst.adv_data_len);
                    ESP_LOGI(TAG, "USER_DATA:");
                    esp_log_buffer_hex(TAG, user_data, user_data_len);
                    // ESP_LOGW(TAG, "RSSI is %d", param->scan_rst.rssi);
                    ESP_LOGI(TAG, "USER_DATA2:");
                    esp_log_buffer_hex("", user_data2, user_data2_len);
#endif // QUEUE

#endif // ndef THROUGHPUT
#ifdef ROUTINGTABLE
#if 0
                    routing_note *new_routing_table_node = (routing_note *)malloc(sizeof(routing_note));
                    memcpy(new_routing_table_node->mac, scan_result->scan_rst.bda, 6);
                    insert_routing_node(&my_routing_table, new_routing_table_node);
                    // print_routing_table(&my_routing_table);
#else
                    insert_routing_node(&my_routing_table, scan_result->scan_rst.bda);
                    print_routing_table(&my_routing_table);
#endif
#endif // ROUTINGTABLE

#ifdef GPIO
                    gpio_set_level(GPIO_OUTPUT_IO_1, flag);
                    flag = !flag;
                    printf("flag is %d\n", flag);
#endif // GPIO
                }
            }
            break;
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
}

#ifdef QUEUE
static void all_queue_init()
{
    queue_init(&rec_queue);
    queue_init(&send_queue);
}
#endif // QUEUE

#ifdef BUTTON
void button_ops()
{
    uint8_t mac_test[] = {
        0xfc, 0xb4, 0x67, 0x50, 0xeb, 0x36};
    remove_routing_node(&my_routing_table, mac_test);
    print_routing_table(&my_routing_table);
}
#endif // BUTTON
#ifdef GPIO
static void esp_gpio_init(void)
{
    // zero-initialize the config structure.
    gpio_config_t io_conf = {};
    // disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    // set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    // bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    // disable pull-down mode
    io_conf.pull_down_en = 0;
    // disable pull-up mode
    io_conf.pull_up_en = 0;
    // configure GPIO with the given settings
    gpio_config(&io_conf);
}
#endif // GPIO
void app_main(void)
{
    esp_err_t ret;
    // 初始化调度锁
    xMutex1 = xSemaphoreCreateMutex();
    if (xMutex1 == NULL) {
        ESP_LOGE(TAG, "semaphore create error");
        return;
    }
#if 0
    xMutex2 = xSemaphoreCreateMutex();
    if (xMutex2 == NULL) {
        ESP_LOGE(TAG, "semaphore create error");
        return;
    }
#endif
    // srand((unsigned int)esp_random());

    // 初始化NV存储
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 初始化BLE控制器和蓝牙堆栈
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "esp_bt_controller_init failed: %s", esp_err_to_name(ret));
        return;
    }
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(TAG, "esp_bt_controller_enable failed: %s", esp_err_to_name(ret));
        return;
    }
    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(TAG, "esp_bluedroid_init failed: %s", esp_err_to_name(ret));
        return;
    }
    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "esp_bluedroid_enable failed: %s", esp_err_to_name(ret));
        return;
    }
    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret) {
        ESP_LOGE(TAG, "gap register error, error code = %x", ret);
        return;
    }
#ifdef GPIO
    esp_gpio_init();
#endif // GPIO
#ifdef ROUTINGTABLE
    init_routing_table(&my_routing_table);
#endif // ROUTINGTABLE
#ifdef QUEUE
    all_queue_init();
#endif // QUEUE
       //  初始广播数据
       //     data_match(adv_data_name, adv_data_ff1);
    // esp_ble_gap_config_adv_data_raw(adv_data_31, 31);
    esp_ble_gap_set_scan_params(&ble_scan_params);
    esp_ble_gap_start_scanning(duration);
    // esp_ble_gap_start_advertising(&adv_params);
// xTaskCreate(switch_adv_data_task, "switch_adv_data_task", 4096, NULL, 5, NULL);
//  xTaskCreate(gpio_task, "gpio_task", 4096, NULL, 5, NULL);
#ifdef QUEUE
    // xTaskCreate(ble_send_data_task, "ble_send_data_task", 512, NULL, 0, NULL);
    xTaskCreate(ble_rec_data_task, "ble_rec_data_task", 2048, NULL, 0, NULL);
#endif // QUEUE
#ifdef BUTTON
    board_init();
#endif // BUTTION
#ifdef THROUGHPUT
    xTaskCreate(throughput_task, "throughput_task", 4096, NULL, 5, NULL);
#endif // THROUGHPUT
}
