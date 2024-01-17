
#ifndef BLE_TIMER_H_
#define BLE_TIMER_H_

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
#include "esp_timer.h"
#include "freertos/projdefs.h"

#define BLE_TIMER

#define TIME1_TIMER_PERIOD 5000000 // 10S
#define TIME2_TIMER_PERIOD 5000000 // 20S

extern SemaphoreHandle_t xCountingSemaphore_timeout1;
extern SemaphoreHandle_t xCountingSemaphore_timeout2;

extern esp_timer_handle_t ble_time1_timer;
extern esp_timer_handle_t ble_time2_timer;

extern bool timer1_timeout;
extern bool timer2_timeout;

extern bool timer1_running;
extern bool timer2_running;

void ble_timer_init(void);

void time1_timer_cb(void);

void time2_timer_cb(void);

#endif