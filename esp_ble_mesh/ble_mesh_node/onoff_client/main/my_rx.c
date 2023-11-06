/*
 * @Author: Zhenwei Song zhenwei.song@qq.com
 * @Date: 2023-10-27 14:17:41
 * @LastEditors: Zhenwei Song zhenwei.song@qq.com
 * @LastEditTime: 2023-10-27 15:26:46
 * @FilePath: \esp32\esp_ble_mesh\ble_mesh_node\onoff_client\main\my_rx.c
 * @Description: 仅供学习交流使用
 * Copyright (c) 2023 by Zhenwei Song, All Rights Reserved.
 */

#include "my_rx.h"

bool my_led_test = false;
bool msg_detected = false;

void my_rx_init(void)
{
    const uart_config_t uart_config = {
        .baud_rate = 115200,                   // 设置波特率    115200
        .data_bits = UART_DATA_8_BITS,         // 设置数据位    8位
        .parity = UART_PARITY_DISABLE,         // 设置奇偶校验  不校验
        .stop_bits = UART_STOP_BITS_1,         // 设置停止位    1
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, // 设置硬件流控制 不使能
        .source_clk = UART_SCLK_APB,           // 设置时钟源
    };
    // 安装串口驱动 串口编号、接收buff、发送buff、事件队列、分配中断的标志
    uart_driver_install(UART_NUM_0, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    // 串口参数配置 串口号、串口配置参数
    uart_param_config(UART_NUM_0, &uart_config);
    // 设置串口引脚号 串口编号、tx引脚、rx引脚、rts引脚、cts引脚
    uart_set_pin(UART_NUM_0, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

void my_rx_task(void *arg)
{
    static const char *RX_TASK_TAG = "RX_TASK";         // 接收任务
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);       // 设置log打印
    uint8_t *data = (uint8_t *)malloc(RX_BUF_SIZE + 1); // 申请动态内存
    while (1) {
        const int rxBytes = uart_read_bytes(UART_NUM_0, data, RX_BUF_SIZE, 1000 / portTICK_PERIOD_MS); // 读取数据
        if (rxBytes > 0) {                                                                             // 判断数据长度
            data[rxBytes] = 0;                                                                         // 清空data中的数据
            ESP_LOGI(RX_TASK_TAG, "Read %d bytes: '%s'", rxBytes, data);                               // log打印
            ESP_LOG_BUFFER_HEXDUMP(RX_TASK_TAG, data, rxBytes, ESP_LOG_INFO);                          // 打印16进制数据
            if (data[0] == 0x31) {
                my_led_test = true;
                ESP_LOGI(RX_TASK_TAG, "led_on:%d", my_led_test);
            }
            else if (data[0] == 0x32) {
                my_led_test = false;
                ESP_LOGI(RX_TASK_TAG, "led_on:%d", my_led_test);
            }
            msg_detected = true;
        }
    }
    free(data); // 释放内存
}

