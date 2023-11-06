#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#define TAG "BLE_BROADCAST"

#define ADV_INTERVAL_MS 1000 // 广播间隔时间，单位：毫秒

// 第一段广播数据
static uint8_t adv_data_1[] = {
    0x02, 0x01, 0x06,                                       // Flags
    0x0A, 0x09, 'D', 'A', 'T', 'A', '1', '0', '1', '0', '1' // 自定义数据
};

// 第二段广播数据
static uint8_t adv_data_2[] = {
    0x02, 0x01, 0x06,                                       // Flags
    0x0A, 0x09, 'D', 'A', 'T', 'A', '2', '0', '2', '0', '2' // 自定义数据
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min = ADV_INTERVAL_MS / 0.625,
    .adv_int_max = ADV_INTERVAL_MS / 0.625,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static bool use_adv_data_1 = true; // 初始广播数据为第一段数据

static void switch_adv_data_task(void *pvParameters)
{
    while (1) {
        // 切换广播数据
        if (use_adv_data_1) {
            esp_ble_gap_config_adv_data_raw(adv_data_2, sizeof(adv_data_2));
            use_adv_data_1 = false;
        }
        else {
            esp_ble_gap_config_adv_data_raw(adv_data_1, sizeof(adv_data_1));
            use_adv_data_1 = true;
        }
        // 启动新的广播
        esp_ble_gap_start_advertising(&adv_params);

        // 等待下一次切换
        vTaskDelay(pdMS_TO_TICKS(ADV_INTERVAL_MS));
    }
}

static void ble_scan_task(void *pvParameters)
{
    while (1) {
        // 执行BLE扫描逻辑，监听广播
        // 在这里添加您的BLE扫描代码
        vTaskDelay(pdMS_TO_TICKS(ADV_INTERVAL_MS)); // 等待一段时间后再次扫描
    }
}

void app_main(void)
{
    esp_err_t ret;

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
        ESP_LOGE(TAG, "BLE控制器初始化失败: %s", esp_err_to_name(ret));
        return;
    }
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(TAG, "BLE控制器启用失败: %s", esp_err_to_name(ret));
        return;
    }
    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(TAG, "蓝牙堆栈初始化失败: %s", esp_err_to_name(ret));
        return;
    }
    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "蓝牙堆栈启用失败: %s", esp_err_to_name(ret));
        return;
    }

    // 配置初始广播数据
    esp_ble_gap_config_adv_data_raw(adv_data_1, sizeof(adv_data_1));

    // 创建任务来管理广播数据的交替
    xTaskCreate(switch_adv_data_task, "switch_adv_data_task", 4096, NULL, 5, NULL);

    // 创建任务来执行BLE扫描逻辑
    xTaskCreate(ble_scan_task, "ble_scan_task", 4096, NULL, 5, NULL);
