/*
 * @Author: Zhenwei Song zhenwei.song@qq.com
 * @Date: 2023-09-22 17:13:32
 * @LastEditors: Zhenwei-Song zhenwei.song@qq.com
 * @LastEditTime: 2023-11-01 16:49:40
 * @FilePath: \esp32\gatt_server_service_table_modified\main\gatts_table_creat_demo.c
 * @Description:
 * 该代码用于接收测试（循环发送01到0f的包）
 * 实现了广播与扫描同时进行（基于gap层）
 * 添加了GPIO的测试内容（由宏定义控制是否启动）
 * 实现了基于adtype中name的判断接收包的方法
 * 实现了字符串的拼接（帧头 + 实际内容）
 * 添加了吞吐量测试
 * Copyright (c) 2023 by Zhenwei Song, All Rights Reserved.
 */
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
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

// #define GPIO //GPIO相关
// #define THROUGHPUT//吞吐量测试
#define NEIGHBOURTABLE // 邻居表（链表）

#define TAG "BLE_BROADCAST"
#define NEIGHBOUR_TAG "NEIGHBOUR_TABLE"

#define ADV_INTERVAL_MS 500 // 广播间隔时间，单位：毫秒
#define SCAN_INTERVAL_MS 225
#define DATA_INTERVAL_MS 600

// #define MAX_DELAY_MS 1000
// #define MIN_DELAY_MS 500

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

uint32_t duration = 0;

// static bool adv_data_changed = false;

static const char remote_device_name[] = "OLTHR";

static uint8_t data_1_size = 0;
static uint8_t data_2_size = 0;

static uint8_t adv_data_name[] = {
    /* device name */
    0x06, 0x09, 'O', 'L', 'T', 'H', 'R'};

static uint8_t adv_data_ff1[] = {
    /*自定义数据段1*/
    0x08, 0xf1, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01};
#if 1 // 31字节数据
static uint8_t adv_data_31[] = {
    /* device name */
    0x06, 0x09, 'O', 'L', 'T', 'H', 'R',
    /*自定义数据段2*/
    0x17,
    0xff, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16};

static uint8_t adv_data_62[] = {
    /* device name */
    0x06, 0x09, 'O', 'L', 'T', 'H', 'R',
    /*自定义数据段2*/
    0x36,
    0xff, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09};
#endif
static uint8_t adv_data_final[31] = {0};

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

#ifdef NEIGHBOURTABLE
typedef struct neighbour_table {
    void *mac;
    // uint8_t mac[6];
    struct neighbour_table *next;
} neighbour, *p_neighbour;

p_neighbour head = NULL;

/**
 * @description: 插入邻居链表
 * @param {p_neighbour} new_neighbour
 * @return {*}
 */
static int insert_neighbour(p_neighbour new_neighbour)
{
    p_neighbour prev = NULL;
    int repeated = 0;

    if (head == NULL) {
        head = new_neighbour;
        new_neighbour->next = NULL;
    }
    else {
        prev = head;
        while (prev != NULL) {
            repeated = memcmp(prev->mac, new_neighbour->mac, 6);
            if (repeated == 0) { // 检查重复
                ESP_LOGI(NEIGHBOUR_TAG, "repeated mac address found");
                goto exit;
            }
            if (prev->next == NULL)
                break;
            else
                prev = prev->next;
        }
        prev->next = new_neighbour;
        new_neighbour->next = NULL;
    }
exit:
    return 0;
}

/**
 * @description: 从邻居链表移除邻居
 * @param {p_neighbour} old_neighbour
 * @return {*}
 */
static void remove_neighbour(p_neighbour old_neighbour)
{
    p_neighbour prev = NULL;
    if (head == old_neighbour) {
        head = head->next;
    }
    else {
        prev = head;
        while (prev != NULL) {
            if (prev->next == NULL)
                break;
            else
                prev = prev->next;
        }
        prev->next = old_neighbour->next;
    }
}

/**
 * @description: 打印邻居表
 * @return {*}
 */
static void print_neighbour_table(void)
{
    p_neighbour temp = head;
    while (temp != NULL) {
        ESP_LOGI(NEIGHBOUR_TAG, "MAC address:");
        esp_log_buffer_hex(NEIGHBOUR_TAG, temp->mac, 6);
        temp = temp->next;
    }
}

#endif // NEIGHBOURTABLE
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

#if 0  /*printf阻塞测试*/
static void gpio_task(void *pvParameters)
{
    int cnt = 0;
    while (1) {
        gpio_set_level(GPIO_OUTPUT_IO_0, 1);
        ESP_LOGI(TAG, "PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_PRINT_TEST_");
        gpio_set_level(GPIO_OUTPUT_IO_0, 0);
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}
#endif /*printf阻塞测试*/

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
    static uint8_t(*adv_data_old)[62] = {0};

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
            user_data = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv,
                                                 ESP_BLE_AD_TYPE_USER_1, &user_data_len); // 解析用户数据1
            user_data2 = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv,
                                                  ESP_BLE_AD_TYPE_USER_2, &user_data2_len); // 解析用户数据2
            //            ESP_LOGI(TAG, "searched Device Name Len %d", adv_name_len);
            //            esp_log_buffer_char(TAG, adv_name, adv_name_len); // 打印设备名

            //            ESP_LOGI(TAG, "\n");
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
/*对接收到的广播包检查重复性*/
#if 0
                    if (adv_data_old == &scan_result->scan_rst.ble_adv) {
                        ESP_LOGW(TAG, "repeated message");
                        break;
                    }
                    ESP_LOGW(TAG, "new message");
                    adv_data_old = &scan_result->scan_rst.ble_adv;
#endif /*对接收到的广播包检查重复性*/
#ifndef THROUGHPUT
                    ESP_LOGW(TAG, "%s is found", remote_device_name);
#ifdef GPIO
                    gpio_set_level(GPIO_OUTPUT_IO_1, flag);
                    flag = !flag;
                    printf("flag is %d\n", flag);
#endif // GPIO
                    ESP_LOGI(TAG, "MAC:");
                    esp_log_buffer_hex(TAG, scan_result->scan_rst.bda, 6); // 打印扫描到设备的MAC地址
                    // 打印广播数据
                    ESP_LOGI(TAG, "ADV_DATA:");
                    esp_log_buffer_hex("", param->scan_rst.ble_adv, param->scan_rst.adv_data_len);
                    ESP_LOGI(TAG, "USER_DATA:");
                    esp_log_buffer_hex("", user_data, user_data_len);
#if 0
                    ESP_LOGI(TAG, "USER_DATA2:");
                    esp_log_buffer_hex("", user_data2, user_data2_len);
#endif
#endif // ndef THROUGHPUT
#ifdef NEIGHBOURTABLE
                    uint8_t mac_addr[6] = {0};
                    memcpy(mac_addr, scan_result->scan_rst.bda, 6);
                    ESP_LOGI(TAG, "mac_addr:");
                    esp_log_buffer_hex(TAG, mac_addr, 6);
                    neighbour new_neighbour = {{mac_addr}, NULL};
                    insert_neighbour(&new_neighbour);
                    print_neighbour_table();
#endif // NEIGHBOURTABLE
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
#endif
    // 初始广播数据
    //    data_match(adv_data_name, adv_data_ff1);
    esp_ble_gap_config_adv_data_raw(adv_data_31, 31);
    esp_ble_gap_set_scan_params(&ble_scan_params);
    esp_ble_gap_start_scanning(duration);
    esp_ble_gap_start_advertising(&adv_params);
    // xTaskCreate(switch_adv_data_task, "switch_adv_data_task", 4096, NULL, 5, NULL);
    //  xTaskCreate(gpio_task, "gpio_task", 4096, NULL, 5, NULL);
#ifdef THROUGHPUT
    xTaskCreate(throughput_task, "throughput_task", 4096, NULL, 5, NULL);
#endif // THROUGHPUT
}
