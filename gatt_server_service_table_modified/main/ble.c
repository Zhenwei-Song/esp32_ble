/*
 * @Author: Zhenwei Song zhenwei.song@qq.com
 * @Date: 2023-09-22 17:13:32
 * @LastEditors: Zhenwei-Song zhenwei.song@qq.com
 * @LastEditTime: 2023-11-20 14:00:44
 * @FilePath: \esp32\gatt_server_service_table_modified\main\ble.c
 * @Description:
 * 该代码用于接收测试（循环发送01到0f的包）
 * 实现了广播与扫描同时进行（基于gap层）
 * 添加了GPIO的测试内容（由宏定义控制是否启动）
 * 实现了基于adtype中name的判断接收包的方法
 * 实现了字符串的拼接（帧头 + 实际内容）
 * 添加了吞吐量测试
 * 添加了rssi
 * 添加了发送和接收消息队列，用单独的task进行处理
 * 添加了路由表，添加了路由表的刷新机制
 * Copyright (c) 2023 by Zhenwei Song, All Rights Reserved.
 */

#include "ble.h"

#ifdef GPIO
#include "driver/gpio.h"
#endif // GPIO
#ifdef QUEUE
#include "ble_queue.h"
#endif // QUEUE
#ifdef ROUTINGTABLE
#include "esp_mac.h"
#include "routing_table.h"
#endif // ROUTINGTABLE
#ifdef BUTTON
#include "board.h"
#endif

#include "data_manage.h"

#define REFRESH_ROUTING_TABLE_TIME 1000
#define ADV_TIME 2000
#define REC_TIME 100

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

#ifdef QUEUE
queue rec_queue;
queue send_queue;
#endif // QUEUE

#ifdef ROUTINGTABLE
static void ble_routing_table_task(void *pvParameters)
{
    while (1) {
        refresh_cnt_routing_table(&neighbor_table, &my_information);
#ifndef SELF_ROOT
        update_quality_of_routing_table(&neighbor_table);
        set_my_next_id_quality_and_distance(&neighbor_table, &my_information);
#endif
        print_routing_table(&neighbor_table);
#if 1
        if (refresh_flag == true) { // 状态改变，立即发送hello
            refresh_flag = false;
            memcpy(adv_data_final, data_match(adv_data_name_7, generate_phello(&my_information), HEAD_DATA_LEN, PHELLO_FINAL_DATA_LEN), FINAL_DATA_LEN);
            esp_ble_gap_config_adv_data_raw(adv_data_final, 31);
            esp_ble_gap_start_advertising(&adv_params);
        }
#endif
#if 1
        ESP_LOGW(DATA_TAG, "****************************Start printing my info:***********************************************");
        ESP_LOGI(DATA_TAG, "root_id:");
        esp_log_buffer_hex(DATA_TAG, my_information.root_id, ID_LEN);
        ESP_LOGI(DATA_TAG, "is_root:%d", my_information.is_root);
        ESP_LOGI(DATA_TAG, "is_connected:%d", my_information.is_connected);
        ESP_LOGI(DATA_TAG, "next_id:");
        esp_log_buffer_hex(DATA_TAG, my_information.next_id, ID_LEN);
        ESP_LOGI(DATA_TAG, "distance:%d", my_information.distance);
        ESP_LOGI(DATA_TAG, "quality:");
        esp_log_buffer_hex(DATA_TAG, my_information.quality, QUALITY_LEN);
        ESP_LOGI(DATA_TAG, "update:%d", my_information.update);
        ESP_LOGW(DATA_TAG, "****************************Printing my info is finished *****************************************");
#endif
        vTaskDelay(pdMS_TO_TICKS(REFRESH_ROUTING_TABLE_TIME));
    }
}
#endif // ROUTINGTABLE

#ifdef QUEUE
static void ble_rec_data_task(void *pvParameters)
{
    uint8_t *phello = NULL;
    uint8_t *main_quest = NULL;
    uint8_t *main_answer = NULL;
    uint8_t *second_quest = NULL;
    uint8_t *second_answer = NULL;
    uint8_t *repair = NULL;
    uint8_t *rec_data = NULL;
    uint8_t phello_len = 0;
    uint8_t main_quest_len = 0;
    uint8_t main_answer_len = 0;
    uint8_t second_quest_len = 0;
    uint8_t second_answer_len = 0;
    uint8_t repair_len = 0;
    while (1) {
        if (!queue_is_empty(&rec_queue)) {
            rec_data = queue_pop(&rec_queue);
            if (rec_data != NULL) {
                phello = esp_ble_resolve_adv_data(rec_data, ESP_BLE_AD_TYPE_PHELLO, &phello_len);
                main_quest = esp_ble_resolve_adv_data(rec_data, ESP_BLE_AD_TYPE_ANHSP, &main_quest_len);
                main_answer = esp_ble_resolve_adv_data(rec_data, ESP_BLE_AD_TYPE_HSRREP, &main_answer_len);
                second_quest = esp_ble_resolve_adv_data(rec_data, ESP_BLE_AD_TYPE_ANRREQ, &second_quest_len);
                second_answer = esp_ble_resolve_adv_data(rec_data, ESP_BLE_AD_TYPE_ANRREP, &second_answer_len);
                repair = esp_ble_resolve_adv_data(rec_data, ESP_BLE_AD_TYPE_RERR, &repair_len);
                // ESP_LOGI(TAG, "ADV_DATA:");
                // esp_log_buffer_hex(TAG, rec_data, 31);
                if (phello != NULL) {
                    resolve_phello(phello, &my_information, temp_rssi);
                    ESP_LOGI(TAG, "PHELLO_DATA:");
                    esp_log_buffer_hex(TAG, phello, phello_len);
                    ESP_LOGE(TAG, "rssi:%d", temp_rssi);
                }
                if (main_quest != NULL) {
                    ESP_LOGI(TAG, "MAIN_QUEST_DATA:");
                    esp_log_buffer_hex(TAG, main_quest, main_quest_len);
                }
                if (main_answer != NULL) {
                    ESP_LOGI(TAG, "MAIN_ANSWER_DATA:");
                    esp_log_buffer_hex(TAG, main_answer, main_answer_len);
                }
                if (second_quest != NULL) {
                    ESP_LOGI(TAG, "SECOND_QUEST_DATA:");
                    esp_log_buffer_hex(TAG, second_quest, second_quest_len);
                }
                if (second_answer != NULL) {
                    ESP_LOGI(TAG, "SECOND_ANSWER_DATA:");
                    esp_log_buffer_hex(TAG, second_answer, second_answer_len);
                }
                if (repair != NULL) {
                    ESP_LOGI(TAG, "REPAIR_DATA:");
                    esp_log_buffer_hex(TAG, repair, repair_len);
                }
                free(rec_data);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(REC_TIME));
    }
}

static void ble_send_data_task(void *pvParameters)
{
    uint8_t *send_data = NULL;
    while (1) {

        memcpy(adv_data_final, data_match(adv_data_name_7, generate_phello(&my_information), HEAD_DATA_LEN, PHELLO_FINAL_DATA_LEN), FINAL_DATA_LEN);
        queue_push(&send_queue, adv_data_final, 0);
        queue_push(&send_queue, adv_data_final, 0);
        // queue_push(&send_queue, adv_data_final);

        if (!queue_is_empty(&send_queue)) {
            send_data = queue_pop(&send_queue);
            if (send_data != NULL) {
                esp_ble_gap_config_adv_data_raw(send_data, 31);
                esp_ble_gap_start_advertising(&adv_params);
                free(send_data);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(ADV_TIME));
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
            // ESP_LOGI(TAG, "Advertising start successfully");
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
                    queue_push_with_check(&rec_queue, scan_result->scan_rst.ble_adv, scan_result->scan_rst.rssi);
                    // queue_print(&rec_queue);
#endif // ndef THROUGHPUT

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

#ifdef ROUTINGTABLE
static void routing_table_init(void)
{
    uint8_t my_mac[6];
    init_routing_table(&neighbor_table);
    esp_read_mac(my_mac, ESP_MAC_BT);
    my_info_init(&my_information, my_mac);
}
#endif // ROUTINGTABLE

#ifdef QUEUE
static void all_queue_init(void)
{
    queue_init(&rec_queue);
    queue_init(&send_queue);
}
#endif // QUEUE

#ifdef BUTTON
#if 1
void button_ops()
{
    uint8_t mac_test[] = {
        0xfc, 0xb4, 0x67, 0x50, 0xeb, 0x36};
    remove_routing_node(&neighbor_table, mac_test);
    print_routing_table(&neighbor_table);
}
#else
void button_ops()
{
    static char i;
    adv_data_31[30] = i;
    i++;
    if (i == 16) {
        i = 1;
    }
    esp_ble_gap_config_adv_data_raw(adv_data_31, 31);
    esp_ble_gap_start_advertising(&adv_params);
    // esp_ble_gap_stop_advertising();
}
#endif
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
    routing_table_init();
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
    xTaskCreate(ble_routing_table_task, "ble_routing_table_task", 4096, NULL, 5, NULL);
    xTaskCreate(ble_send_data_task, "ble_send_data_task", 4096, NULL, 5, NULL);
    xTaskCreate(ble_rec_data_task, "ble_rec_data_task", 4096, NULL, 5, NULL);
#endif // QUEUE
#ifdef BUTTON
    board_init();
#endif // BUTTION
#ifdef THROUGHPUT
    xTaskCreate(throughput_task, "throughput_task", 4096, NULL, 5, NULL);
#endif // THROUGHPUT
}
