#include "ble_timer.h"

esp_timer_handle_t ble_time1_timer;
esp_timer_handle_t ble_time2_timer;

bool timer1_timeout = false;
bool timer2_timeout = false;

SemaphoreHandle_t xCountingSemaphore_timeout1;
SemaphoreHandle_t xCountingSemaphore_timeout2;

void ble_timer_init(void)
{
    const esp_timer_create_args_t time1_timer_args = {
        .callback = &time1_timer_cb,
        .name = "timer1"}; // 定时器名字
    ESP_ERROR_CHECK(esp_timer_create(&time1_timer_args, &ble_time1_timer));

    const esp_timer_create_args_t time2_timer_args = {
        .callback = &time2_timer_cb,
        .name = "timer2"}; // 定时器名字
    ESP_ERROR_CHECK(esp_timer_create(&time2_timer_args, &ble_time2_timer));
}

void time1_timer_cb(void)
{
    timer1_timeout = true;
    xSemaphoreGive(xCountingSemaphore_timeout1);
    printf("time1_timeout\n");
}

void time2_timer_cb(void)
{
    timer2_timeout = true;
    xSemaphoreGive(xCountingSemaphore_timeout2);
    printf("time2_timeout\n");
}