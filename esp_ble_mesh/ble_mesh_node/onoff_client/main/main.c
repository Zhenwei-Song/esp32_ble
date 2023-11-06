/* main.c - Application main entry point */

/*
 * SPDX-FileCopyrightText: 2017 Intel Corporation
 * SPDX-FileContributor: 2018-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// #define SERVER
#define VENDOR_SERVER
//#define VENDOR_CLIENT

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_generic_model_api.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_provisioning_api.h"

#include "ble_mesh_example_init.h"
#include "ble_mesh_example_nvs.h"
#include "board.h"
// #include "my_rx.h"

#include "driver/uart.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "EXAMPLE"

#define CID_ESP 0x02E5

#ifdef SERVER
#include "esp_ble_mesh_local_data_operation_api.h"

extern struct _led_state led_state[3];
#endif

#if defined VENDOR_SERVER || defined VENDOR_CLIENT
#include "esp_timer.h"

#define ESP_BLE_MESH_VND_MODEL_ID_CLIENT 0x0000
#define ESP_BLE_MESH_VND_MODEL_ID_SERVER 0x0001

#define ESP_BLE_MESH_VND_MODEL_OP_SEND ESP_BLE_MESH_MODEL_OP_3(0x00, CID_ESP)
#define ESP_BLE_MESH_VND_MODEL_OP_STATUS ESP_BLE_MESH_MODEL_OP_3(0x01, CID_ESP)

#define MSG_TIMEOUT 0
#define MSG_ROLE 0

static uint16_t vnd_tid;
#endif // VENDOR_SERVER || VENDOR_CLIENT

static uint8_t dev_uuid[16] = {0xdd, 0xdd};

static struct example_info_store {
    uint16_t net_idx; /* NetKey Index */
    uint16_t app_idx; /* AppKey Index */
    uint8_t onoff;    /* Remote OnOff */
    uint8_t tid;      /* Message TID */
} __attribute__((packed)) store = {
    .net_idx = ESP_BLE_MESH_KEY_UNUSED,
    .app_idx = ESP_BLE_MESH_KEY_UNUSED,
    .onoff = LED_OFF,
    .tid = 0x0,
};

static nvs_handle_t NVS_HANDLE; // 用于断电后保存数据
static const char *NVS_KEY = "onoff_client";

static esp_ble_mesh_client_t onoff_client;

static esp_ble_mesh_cfg_srv_t config_server = {
    .relay = ESP_BLE_MESH_RELAY_DISABLED,
    .beacon = ESP_BLE_MESH_BEACON_ENABLED,
#if defined(CONFIG_BLE_MESH_FRIEND)
    .friend_state = ESP_BLE_MESH_FRIEND_ENABLED,
#else
    .friend_state = ESP_BLE_MESH_FRIEND_NOT_SUPPORTED,
#endif
#if defined(CONFIG_BLE_MESH_GATT_PROXY_SERVER)
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_ENABLED,
#else
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_NOT_SUPPORTED,
#endif
    .default_ttl = 7,
    /* 3 transmissions with 20ms interval */
    .net_transmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .relay_retransmit = ESP_BLE_MESH_TRANSMIT(2, 20),
};

ESP_BLE_MESH_MODEL_PUB_DEFINE(onoff_cli_pub, 2 + 1, ROLE_NODE);

#ifdef SERVER
ESP_BLE_MESH_MODEL_PUB_DEFINE(onoff_pub_0, 2 + 3, ROLE_NODE);
static esp_ble_mesh_gen_onoff_srv_t onoff_server_0 = {
    .rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
    .rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
};

ESP_BLE_MESH_MODEL_PUB_DEFINE(onoff_pub_1, 2 + 3, ROLE_NODE);
static esp_ble_mesh_gen_onoff_srv_t onoff_server_1 = {
    .rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
    .rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
};

ESP_BLE_MESH_MODEL_PUB_DEFINE(onoff_pub_2, 2 + 3, ROLE_NODE);
static esp_ble_mesh_gen_onoff_srv_t onoff_server_2 = {
    .rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
    .rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
};
#endif // SERVER

#ifdef VENDOR_SERVER
static esp_ble_mesh_model_op_t vnd_op_server[] = {
    ESP_BLE_MESH_MODEL_OP(ESP_BLE_MESH_VND_MODEL_OP_SEND, 2),
    ESP_BLE_MESH_MODEL_OP_END,
};

static esp_ble_mesh_model_t vnd_models_server[] = {
    ESP_BLE_MESH_VENDOR_MODEL(CID_ESP, ESP_BLE_MESH_VND_MODEL_ID_SERVER,
                              vnd_op_server, NULL, NULL),
};
#endif // VENDOR_SERVER

#ifdef VENDOR_CLIENT
static esp_ble_mesh_client_t config_client;

static const esp_ble_mesh_client_op_pair_t vnd_op_pair[] = {
    {ESP_BLE_MESH_VND_MODEL_OP_SEND, ESP_BLE_MESH_VND_MODEL_OP_STATUS},
};

static esp_ble_mesh_client_t vendor_client = {
    .op_pair_size = ARRAY_SIZE(vnd_op_pair),
    .op_pair = vnd_op_pair,
};

static esp_ble_mesh_model_op_t vnd_op[] = {
    ESP_BLE_MESH_MODEL_OP(ESP_BLE_MESH_VND_MODEL_OP_STATUS, 2),
    ESP_BLE_MESH_MODEL_OP_END,
};

static esp_ble_mesh_model_t vnd_models[] = {
    ESP_BLE_MESH_VENDOR_MODEL(CID_ESP, ESP_BLE_MESH_VND_MODEL_ID_CLIENT,
                              vnd_op, NULL, &vendor_client),
};
#endif // VENDOR_CLIENT

#if defined VENDOR_CLIENT && defined VENDOR_SERVER
static esp_ble_mesh_model_t root_models[] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&config_server),
    ESP_BLE_MESH_MODEL_CFG_CLI(&config_client),
};
#elif defined VENDOR_CLIENT
static esp_ble_mesh_model_t root_models[] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&config_server),
    ESP_BLE_MESH_MODEL_CFG_CLI(&config_client),
};
#elif defined VENDOR_SERVER
static esp_ble_mesh_model_t root_models[] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&config_server),
};
#else
static esp_ble_mesh_model_t root_models[] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&config_server),
    ESP_BLE_MESH_MODEL_GEN_ONOFF_CLI(&onoff_cli_pub, &onoff_client),
};
#endif

#ifdef SERVER
static esp_ble_mesh_model_t extend_model_0[] = {
    ESP_BLE_MESH_MODEL_GEN_ONOFF_SRV(&onoff_pub_0, &onoff_server_0),
};

static esp_ble_mesh_model_t extend_model_1[] = {
    ESP_BLE_MESH_MODEL_GEN_ONOFF_SRV(&onoff_pub_1, &onoff_server_1),
};

static esp_ble_mesh_model_t extend_model_2[] = {
    ESP_BLE_MESH_MODEL_GEN_ONOFF_SRV(&onoff_pub_2, &onoff_server_2),
};

#endif // SERVER

static esp_ble_mesh_elem_t elements[] = {
#if defined VENDOR_SERVER && defined VENDOR_CLIENT
    ESP_BLE_MESH_ELEMENT(0, root_models, vnd_models),
    ESP_BLE_MESH_ELEMENT(0, vnd_models_server, ESP_BLE_MESH_MODEL_NONE),
// ESP_BLE_MESH_ELEMENT(0, vnd_models_server, ESP_BLE_MESH_MODEL_NONE),
#elif defined VENDOR_CLIENT
    ESP_BLE_MESH_ELEMENT(0, root_models, vnd_models),
#elif defined VENDOR_SERVER
    ESP_BLE_MESH_ELEMENT(0, root_models, vnd_models_server),
#else
    ESP_BLE_MESH_ELEMENT(0, root_models, ESP_BLE_MESH_MODEL_NONE),
#endif // VENDOR_SERVER ||VENDOR_CLIENT
#ifdef SERVER
    ESP_BLE_MESH_ELEMENT(0, extend_model_0, ESP_BLE_MESH_MODEL_NONE),
    ESP_BLE_MESH_ELEMENT(0, extend_model_1, ESP_BLE_MESH_MODEL_NONE),
    ESP_BLE_MESH_ELEMENT(0, extend_model_2, ESP_BLE_MESH_MODEL_NONE),
#endif // SERVER
};

static esp_ble_mesh_comp_t composition = {
    .cid = CID_ESP,
    .elements = elements,
    .element_count = ARRAY_SIZE(elements),
};

/* Disable OOB security for SILabs Android app */
static esp_ble_mesh_prov_t provision = {
    .uuid = dev_uuid,
#if 0
    .output_size = 4,
    .output_actions = ESP_BLE_MESH_DISPLAY_NUMBER,
    .input_actions = ESP_BLE_MESH_PUSH,
    .input_size = 4,
#else
    .output_size = 0,
    .output_actions = 0,
#endif
};

#if 0
static const int RX_BUF_SIZE = 1024;
static bool my_led_test = false;

static void my_rx_init(void)
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

#endif

static void mesh_example_info_store(void)
{
    ble_mesh_nvs_store(NVS_HANDLE, NVS_KEY, &store, sizeof(store));
}

static void mesh_example_info_restore(void)
{
    esp_err_t err = ESP_OK;
    bool exist = false;

    err = ble_mesh_nvs_restore(NVS_HANDLE, NVS_KEY, &store, sizeof(store), &exist);
    if (err != ESP_OK) {
        return;
    }

    if (exist) {
        ESP_LOGI(TAG, "Restore, net_idx 0x%04x, app_idx 0x%04x, onoff %u, tid 0x%02x",
                 store.net_idx, store.app_idx, store.onoff, store.tid);
    }
}

static void prov_complete(uint16_t net_idx, uint16_t addr, uint8_t flags, uint32_t iv_index)
{
    ESP_LOGI(TAG, "net_idx: 0x%04x, addr: 0x%04x", net_idx, addr);
    ESP_LOGI(TAG, "flags: 0x%02x, iv_index: 0x%08" PRIx32, flags, iv_index);
    board_led_operation(LED_G, LED_OFF);
    store.net_idx = net_idx;
    /* mesh_example_info_store() shall not be invoked here, because if the device
     * is restarted and goes into a provisioned state, then the following events
     * will come:
     * 1st: ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT
     * 2nd: ESP_BLE_MESH_PROV_REGISTER_COMP_EVT
     * So the store.net_idx will be updated here, and if we store the mesh example
     * info here, the wrong app_idx (initialized with 0xFFFF) will be stored in nvs
     * just before restoring it.
     */
}

static void example_ble_mesh_provisioning_cb(esp_ble_mesh_prov_cb_event_t event,
                                             esp_ble_mesh_prov_cb_param_t *param)
{
    switch (event) {
    case ESP_BLE_MESH_PROV_REGISTER_COMP_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_PROV_REGISTER_COMP_EVT, err_code %d", param->prov_register_comp.err_code);
        mesh_example_info_restore(); /* Restore proper mesh example info */
        break;
    case ESP_BLE_MESH_NODE_PROV_ENABLE_COMP_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_ENABLE_COMP_EVT, err_code %d", param->node_prov_enable_comp.err_code);
        break;
    case ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT, bearer %s",
                 param->node_prov_link_open.bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT");
        break;
    case ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT, bearer %s",
                 param->node_prov_link_close.bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT");
        break;
    case ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT");
        prov_complete(param->node_prov_complete.net_idx, param->node_prov_complete.addr,
                      param->node_prov_complete.flags, param->node_prov_complete.iv_index);
        break;
    case ESP_BLE_MESH_NODE_PROV_RESET_EVT:
        break;
    case ESP_BLE_MESH_NODE_SET_UNPROV_DEV_NAME_COMP_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_SET_UNPROV_DEV_NAME_COMP_EVT, err_code %d", param->node_set_unprov_dev_name_comp.err_code);
        break;
    default:
        break;
    }
}

#ifdef SERVER
static void example_change_led_state(esp_ble_mesh_model_t *model,
                                     esp_ble_mesh_msg_ctx_t *ctx, uint8_t onoff)
{
    uint16_t primary_addr = esp_ble_mesh_get_primary_element_address();
    uint8_t elem_count = esp_ble_mesh_get_element_count();
    struct _led_state *led = NULL;
    uint8_t i;

    if (ESP_BLE_MESH_ADDR_IS_UNICAST(ctx->recv_dst)) {
        for (i = 0; i < elem_count; i++) {
            if (ctx->recv_dst == (primary_addr + i)) {
                led = &led_state[i];
                board_led_operation(led->pin, onoff);
            }
        }
    }
    else if (ESP_BLE_MESH_ADDR_IS_GROUP(ctx->recv_dst)) {
        if (esp_ble_mesh_is_model_subscribed_to_group(model, ctx->recv_dst)) {
            led = &led_state[model->element->element_addr - primary_addr];
            board_led_operation(led->pin, onoff);
        }
    }
    else if (ctx->recv_dst == 0xFFFF) {
        led = &led_state[model->element->element_addr - primary_addr];
        board_led_operation(led->pin, onoff);
    }
}

static void example_handle_gen_onoff_msg(esp_ble_mesh_model_t *model,
                                         esp_ble_mesh_msg_ctx_t *ctx,
                                         esp_ble_mesh_server_recv_gen_onoff_set_t *set)
{
    esp_ble_mesh_gen_onoff_srv_t *srv = model->user_data;

    switch (ctx->recv_op) {
    case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET:
        ESP_LOGW(TAG, "GET_ONOFF");
        esp_ble_mesh_server_model_send_msg(model, ctx,
                                           ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS, sizeof(srv->state.onoff), &srv->state.onoff);
        break;
    case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET:
    case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK:
        if (set->op_en == false) {
            srv->state.onoff = set->onoff;
        }
        else {
            /* TODO: Delay and state transition */
            srv->state.onoff = set->onoff;
        }
        if (ctx->recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET) {
            esp_ble_mesh_server_model_send_msg(model, ctx,
                                               ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS, sizeof(srv->state.onoff), &srv->state.onoff);
        }
        esp_ble_mesh_model_publish(model, ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS,
                                   sizeof(srv->state.onoff), &srv->state.onoff, ROLE_NODE);
        example_change_led_state(model, ctx, srv->state.onoff);
        break;
    default:
        break;
    }
}
#endif

void example_ble_mesh_send_gen_onoff_set(void)
{
    esp_ble_mesh_generic_client_set_state_t set = {0};
    esp_ble_mesh_client_common_param_t common = {0};
    esp_err_t err = ESP_OK;

    common.opcode = ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK;
    common.model = onoff_client.model;
    common.ctx.net_idx = store.net_idx;
    common.ctx.app_idx = store.app_idx;
    common.ctx.addr = 0xFFFF; /* to all nodes */
    common.ctx.send_ttl = 3;
    common.ctx.send_rel = false;
    common.msg_timeout = 0; /* 0 indicates that timeout value from menuconfig will be used */
    common.msg_role = ROLE_NODE;

    set.onoff_set.op_en = false;
    set.onoff_set.onoff = store.onoff;
    set.onoff_set.tid = store.tid++;

    err = esp_ble_mesh_generic_client_set_state(&common, &set);
    if (err) {
        ESP_LOGE(TAG, "Send Generic OnOff Set Unack failed");
        return;
    }

    store.onoff = !store.onoff;
    mesh_example_info_store(); /* Store proper mesh example info */
}

#if 0
void ble_mesh_send_gen_onoff_set(void)
{
    esp_ble_mesh_generic_client_set_state_t set = {0};
    esp_ble_mesh_client_common_param_t common = {0};
    esp_err_t err = ESP_OK;

    common.opcode = ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK;
    common.model = onoff_client.model;
    common.ctx.net_idx = store.net_idx;
    common.ctx.app_idx = store.app_idx;
    common.ctx.addr = 0xFFFF; /* to all nodes */
    common.ctx.send_ttl = 3;
    common.ctx.send_rel = false;
    common.msg_timeout = 0; /* 0 indicates that timeout value from menuconfig will be used */
    common.msg_role = ROLE_NODE;

    store.onoff = my_led_test;
    ESP_LOGW(TAG, "set onoff to %d", my_led_test);
    set.onoff_set.op_en = false;
    set.onoff_set.onoff = store.onoff;
    set.onoff_set.tid = store.tid++;

    err = esp_ble_mesh_generic_client_set_state(&common, &set);
    if (err) {
        ESP_LOGE(TAG, "Send Generic OnOff Set Unack failed");
        return;
    }
    else {
        ESP_LOGW(TAG, "Send Generic OnOff Set Unack succeeded");
    }
}

static void my_rx_task(void *arg)
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
            ble_mesh_send_gen_onoff_set();
        }
    }
    free(data); // 释放内存
}

#endif
#if 0
static void msg_send_task(void *arg)
{
    static uint8_t my_onoff = 0;

    esp_ble_mesh_generic_client_set_state_t set = {0};
    esp_ble_mesh_client_common_param_t common = {0};
    esp_err_t err = ESP_OK;

    common.opcode = ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET;
    common.model = onoff_client.model;
    common.ctx.net_idx = store.net_idx;
    common.ctx.app_idx = store.app_idx;
    // common.ctx.addr = 0xFFFF; /* to all nodes */
    common.ctx.addr = 0xC001;
    common.ctx.send_ttl = 3;
    common.ctx.send_rel = false;
    common.msg_timeout = 0; /* 0 indicates that timeout value from menuconfig will be used */
    common.msg_role = ROLE_NODE;

    set.onoff_set.op_en = false;
    while (1) {
        set.onoff_set.onoff = my_onoff;
        set.onoff_set.tid = store.tid++;
        err = esp_ble_mesh_generic_client_set_state(&common, &set);
        if (err) {
            ESP_LOGE(TAG, "Send Generic OnOff Set Unack failed,%d", err);

        }
        my_onoff = !my_onoff;
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
#else
static void msg_send_task(void *arg)
{
    esp_ble_mesh_msg_ctx_t ctx = {0};
    uint32_t opcode;
    esp_err_t err;

    ctx.net_idx = store.net_idx;
    ctx.app_idx = store.app_idx;
    ctx.addr = 0xc000;
    ctx.send_ttl = 3;
    ctx.send_rel = false;
    opcode = ESP_BLE_MESH_VND_MODEL_OP_SEND;
    while (1) {
        if (0 == false) {
            vnd_tid++;
        }
#ifdef VENDOR_CLIENT
        err = esp_ble_mesh_client_model_send_msg(vendor_client.model, &ctx, opcode,
                                                 sizeof(vnd_tid), (uint8_t *)&vnd_tid, MSG_TIMEOUT, false, MSG_ROLE);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send vendor message");
            printf("error: %d\n", err);
        }
#else
        esp_err_t err = esp_ble_mesh_server_model_send_msg(&vnd_models_server[0],
                                                           &ctx, opcode,
                                                           sizeof(vnd_tid), (uint8_t *)&vnd_tid);
        if (err) {
            ESP_LOGE(TAG, "Failed to send message 0x%06x", ESP_BLE_MESH_VND_MODEL_OP_SEND);
            printf("error: %d\n", err);
        }
#endif
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
#endif
static void example_ble_mesh_generic_client_cb(esp_ble_mesh_generic_client_cb_event_t event,
                                               esp_ble_mesh_generic_client_cb_param_t *param)
{
    ESP_LOGI(TAG, "Generic client, event %u, error code %d, opcode is 0x%04" PRIx32,
             event, param->error_code, param->params->opcode);

    switch (event) {
    case ESP_BLE_MESH_GENERIC_CLIENT_GET_STATE_EVT: // 表示客户端成功获取了一个服务器的状态信息
        ESP_LOGI(TAG, "ESP_BLE_MESH_GENERIC_CLIENT_GET_STATE_EVT");
        if (param->params->opcode == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET) {
            ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET, onoff %d", param->status_cb.onoff_status.present_onoff);
        }
        break;
    case ESP_BLE_MESH_GENERIC_CLIENT_SET_STATE_EVT: // 当客户端成功设置了服务器的状态信息时触发
        ESP_LOGI(TAG, "ESP_BLE_MESH_GENERIC_CLIENT_SET_STATE_EVT");
        if (param->params->opcode == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET) {
            ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET, onoff %d", param->status_cb.onoff_status.present_onoff);
        }
        break;
    case ESP_BLE_MESH_GENERIC_CLIENT_PUBLISH_EVT: // 表示接收了一个publish
        ESP_LOGI(TAG, "ESP_BLE_MESH_GENERIC_CLIENT_PUBLISH_EVT");
        break;
    case ESP_BLE_MESH_GENERIC_CLIENT_TIMEOUT_EVT: // 当操作超时未收到响应时触发
        ESP_LOGI(TAG, "ESP_BLE_MESH_GENERIZC_CLIENT_TIMEOUT_EVT");
        if (param->params->opcode == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET) {
            /* If failed to get the response of Generic OnOff Set, resend Generic OnOff Set  */
            example_ble_mesh_send_gen_onoff_set();
        }
        break;
    default:
        break;
    }
}

#ifdef SERVER
static void example_ble_mesh_generic_server_cb(esp_ble_mesh_generic_server_cb_event_t event,
                                               esp_ble_mesh_generic_server_cb_param_t *param)
{
    esp_ble_mesh_gen_onoff_srv_t *srv;
    ESP_LOGI(TAG, "event 0x%02x, opcode 0x%04" PRIx32 ", src 0x%04x, dst 0x%04x",
             event, param->ctx.recv_op, param->ctx.addr, param->ctx.recv_dst);

    switch (event) {
    case ESP_BLE_MESH_GENERIC_SERVER_STATE_CHANGE_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_GENERIC_SERVER_STATE_CHANGE_EVT");
        if (param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET ||
            param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK) {
            ESP_LOGI(TAG, "onoff 0x%02x", param->value.state_change.onoff_set.onoff);
            example_change_led_state(param->model, &param->ctx, param->value.state_change.onoff_set.onoff);
        }
        break;
    case ESP_BLE_MESH_GENERIC_SERVER_RECV_GET_MSG_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_GENERIC_SERVER_RECV_GET_MSG_EVT");
        if (param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET) {
            srv = param->model->user_data;
            ESP_LOGI(TAG, "onoff 0x%02x", srv->state.onoff);
            example_handle_gen_onoff_msg(param->model, &param->ctx, NULL);
        }
        break;
    case ESP_BLE_MESH_GENERIC_SERVER_RECV_SET_MSG_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_GENERIC_SERVER_RECV_SET_MSG_EVT");
        if (param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET ||
            param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK) {
            ESP_LOGI(TAG, "onoff 0x%02x, tid 0x%02x", param->value.set.onoff.onoff, param->value.set.onoff.tid);
            if (param->value.set.onoff.op_en) {
                ESP_LOGI(TAG, "trans_time 0x%02x, delay 0x%02x",
                         param->value.set.onoff.trans_time, param->value.set.onoff.delay);
            }
            example_handle_gen_onoff_msg(param->model, &param->ctx, &param->value.set.onoff);
        }
        break;
    default:
        ESP_LOGE(TAG, "Unknown Generic Server event 0x%02x", event);
        break;
    }
}
#endif

#if defined VENDOR_CLIENT || defined VENDOR_SERVER
void example_ble_mesh_send_vendor_message(bool resend)
{
    esp_ble_mesh_msg_ctx_t ctx = {0};
    uint32_t opcode;
    esp_err_t err;

    ctx.net_idx = store.net_idx;
    ctx.app_idx = store.app_idx;
    ctx.addr = 0xc002;
    ctx.send_ttl = 1;
    ctx.send_rel = 0;
    opcode = ESP_BLE_MESH_VND_MODEL_OP_SEND;

    if (resend == false) {
        vnd_tid++;
    }
#ifdef VENDOR_CLIENT
    err = esp_ble_mesh_client_model_send_msg(vendor_client.model, &ctx, opcode,
                                             sizeof(vnd_tid), (uint8_t *)&vnd_tid, MSG_TIMEOUT, true, MSG_ROLE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send vendor message 0x%06" PRIx32, opcode);
        return;
    }
#else
    err = esp_ble_mesh_server_model_send_msg(&vnd_models_server[0],
                                             &ctx, ESP_BLE_MESH_VND_MODEL_OP_STATUS,
                                             sizeof(vnd_tid), (uint8_t *)&vnd_tid);
    if (err) {
        ESP_LOGE(TAG, "Failed to send message 0x%06x", ESP_BLE_MESH_VND_MODEL_OP_STATUS);
    }
#endif // VENDOR_CLIENT
    // mesh_example_info_store(); /* Store proper mesh example info */
}

static void example_ble_mesh_custom_model_cb(esp_ble_mesh_model_cb_event_t event,
                                             esp_ble_mesh_model_cb_param_t *param)
{
    static int64_t start_time;

    switch (event) {
    case ESP_BLE_MESH_MODEL_OPERATION_EVT:
        ESP_LOGW(TAG, "Recv message!!!!!!!!!!!!!!!");
        if (param->model_operation.opcode == ESP_BLE_MESH_VND_MODEL_OP_STATUS) {
            int64_t end_time = esp_timer_get_time();
            ESP_LOGI(TAG, "STATUS Recv 0x06%" PRIx32 ", tid 0x%04x, time %lldus",
                     param->model_operation.opcode, vnd_tid, end_time - start_time);
        }
        if (param->model_operation.opcode == ESP_BLE_MESH_VND_MODEL_OP_SEND) {
            uint16_t tid = *(uint16_t *)param->model_operation.msg;
            ESP_LOGI(TAG, "SEND Recv 0x%06" PRIx32 ", tid 0x%04x", param->model_operation.opcode, tid);
#ifdef VENDOR_CLIENT
            esp_err_t err = esp_ble_mesh_client_model_send_msg(vendor_client.model,
                                                               param->model_operation.ctx, ESP_BLE_MESH_VND_MODEL_OP_STATUS,
                                                               sizeof(tid), (uint8_t *)&tid, MSG_TIMEOUT, true, MSG_ROLE);
#else
            esp_err_t err = esp_ble_mesh_server_model_send_msg(&vnd_models_server[0],
                                                               param->model_operation.ctx, ESP_BLE_MESH_VND_MODEL_OP_STATUS,
                                                               sizeof(tid), (uint8_t *)&tid);
#endif
            if (err) {
                ESP_LOGE(TAG, "Failed to send message 0x%06x", ESP_BLE_MESH_VND_MODEL_OP_STATUS);
            }
        }
        break;
    case ESP_BLE_MESH_MODEL_SEND_COMP_EVT:
        if (param->model_send_comp.err_code) {
            ESP_LOGE(TAG, "Failed to send message 0x%06" PRIx32, param->model_send_comp.opcode);
            break;
        }
        start_time = esp_timer_get_time();
        ESP_LOGI(TAG, "Send 0x%06" PRIx32, param->model_send_comp.opcode);
        break;
    case ESP_BLE_MESH_CLIENT_MODEL_RECV_PUBLISH_MSG_EVT:
        ESP_LOGI(TAG, "Receive publish message 0x%06" PRIx32, param->client_recv_publish_msg.opcode);
        printf("addr 0x%04x, recv_dst 0x%04x, msg %d\n",
               param->client_recv_publish_msg.ctx->addr,
               param->client_recv_publish_msg.ctx->recv_dst,
               (int)param->client_recv_publish_msg.msg);
#if 1
        if (param->model_operation.opcode == ESP_BLE_MESH_VND_MODEL_OP_STATUS) {
            int64_t end_time = esp_timer_get_time();
            ESP_LOGI(TAG, "STATUS Recv 0x06%" PRIx32 ", tid 0x%04x, time %lldus",
                     param->model_operation.opcode, vnd_tid, end_time - start_time);
        }
        if (param->model_operation.opcode == ESP_BLE_MESH_VND_MODEL_OP_SEND) {
            uint16_t tid = *(uint16_t *)param->model_operation.msg;
            ESP_LOGI(TAG, "SEND Recv 0x%06" PRIx32 ", tid 0x%04x", param->model_operation.opcode, tid);
#ifdef VENDOR_CLIENT
            esp_err_t err = esp_ble_mesh_client_model_send_msg(vendor_client.model,
                                                               param->model_operation.ctx, ESP_BLE_MESH_VND_MODEL_OP_STATUS,
                                                               sizeof(tid), (uint8_t *)&tid, MSG_TIMEOUT, true, MSG_ROLE);
#else
            esp_err_t err = esp_ble_mesh_server_model_send_msg(&vnd_models_server[0],
                                                               param->model_operation.ctx, ESP_BLE_MESH_VND_MODEL_OP_STATUS,
                                                               sizeof(tid), (uint8_t *)&tid);
#endif
            if (err) {
                ESP_LOGE(TAG, "Failed to send message 0x%06x", ESP_BLE_MESH_VND_MODEL_OP_STATUS);
            }
        }
#endif
        break;
    case ESP_BLE_MESH_CLIENT_MODEL_SEND_TIMEOUT_EVT:
        ESP_LOGW(TAG, "Client message 0x%06" PRIx32 " timeout", param->client_send_timeout.opcode);
        example_ble_mesh_send_vendor_message(true);
        break;
    default:
        break;
    }
}
#endif // VENDOR_CLIENT ||VENDOR_SERVER
static void example_ble_mesh_config_server_cb(esp_ble_mesh_cfg_server_cb_event_t event,
                                              esp_ble_mesh_cfg_server_cb_param_t *param)
{
    if (event == ESP_BLE_MESH_CFG_SERVER_STATE_CHANGE_EVT) {
        switch (param->ctx.recv_op) {
        case ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD:
            ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD");
            ESP_LOGI(TAG, "net_idx 0x%04x, app_idx 0x%04x",
                     param->value.state_change.appkey_add.net_idx,
                     param->value.state_change.appkey_add.app_idx);
            ESP_LOG_BUFFER_HEX("AppKey", param->value.state_change.appkey_add.app_key, 16);
            break;
        case ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND:
            ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND");
            ESP_LOGI(TAG, "elem_addr 0x%04x, app_idx 0x%04x, cid 0x%04x, mod_id 0x%04x",
                     param->value.state_change.mod_app_bind.element_addr,
                     param->value.state_change.mod_app_bind.app_idx,
                     param->value.state_change.mod_app_bind.company_id,
                     param->value.state_change.mod_app_bind.model_id);
#if 0
            if (param->value.state_change.mod_app_bind.company_id == 0xFFFF &&
                param->value.state_change.mod_app_bind.model_id == ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_CLI) {
                store.app_idx = param->value.state_change.mod_app_bind.app_idx;
                mesh_example_info_store(); /* Store proper mesh example info */
            }
#endif
            break;
        default:
            break;
        }
    }
}

static esp_err_t ble_mesh_init(void)
{
    esp_err_t err = ESP_OK;

    esp_ble_mesh_register_prov_callback(example_ble_mesh_provisioning_cb);
    // esp_ble_mesh_register_generic_client_callback(example_ble_mesh_generic_client_cb);
#ifdef SERVER
    esp_ble_mesh_register_generic_server_callback(example_ble_mesh_generic_server_cb);
#endif // SERVER
#if defined VENDOR_SERVER || defined VENDOR_CLIENT
    esp_ble_mesh_register_custom_model_callback(example_ble_mesh_custom_model_cb);
#endif // VENDOR_SERVER ||VENDOR_CLIENT
    esp_ble_mesh_register_config_server_callback(example_ble_mesh_config_server_cb);
    err = esp_ble_mesh_init(&provision, &composition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize mesh stack (err %d)", err);
        return err;
    }
#ifdef VENDOR_CLIENT
    err = esp_ble_mesh_client_model_init(&vnd_models[0]);
    if (err) {
        ESP_LOGE(TAG, "Failed to initialize vendor client");
        return err;
    }
#endif // VENDOR_CLIENT
    err = esp_ble_mesh_node_prov_enable(ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable mesh node (err %d)", err);
        return err;
    }

    ESP_LOGI(TAG, "BLE Mesh Node initialized");

    board_led_operation(LED_G, LED_ON);

    return err;
}

void app_main(void)
{
    esp_err_t err;

    ESP_LOGI(TAG, "Initializing...");

    board_init();

    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    err = bluetooth_init();
    if (err) {
        ESP_LOGE(TAG, "esp32_bluetooth_init failed (err %d)", err);
        return;
    }

    /* Open nvs namespace for storing/restoring mesh example info */
    err = ble_mesh_nvs_open(&NVS_HANDLE);
    if (err) {
        return;
    }

    ble_mesh_get_dev_uuid(dev_uuid);

    /* Initialize the Bluetooth Mesh Subsystem */
    err = ble_mesh_init();
    if (err) {
        ESP_LOGE(TAG, "Bluetooth mesh init failed (err %d)", err);
    }
    //    my_rx_init();
    xTaskCreate(msg_send_task, "msg_send_task", 1024 * 2, NULL, 5, NULL); // 接收任务
}