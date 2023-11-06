/*
 * @Author: Zhenwei-Song zhenwei.song@qq.com
 * @Date: 2023-10-30 20:37:31
 * @LastEditors: Zhenwei-Song zhenwei.song@qq.com
 * @LastEditTime: 2023-11-06 18:57:47
 * @FilePath: \esp32\esp_ble_mesh\ble_mesh_vendor_model\vendor_server\main\main.c
 * @Description: 仅供学习交流使用
 * 添加了基于server的msg_sesnd task,2秒发送一次
 * 可以和vendor_client以及server之间互相通信
 * 尝试添加vendor_client(用宏定义控制)，注意：client可给server发消息，server可给server发消息。其他情况发消息，对方无法接受
 * Copyright (c) 2023 by Zhenwei-Song, All Rights Reserved.
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "esp_bt.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_local_data_operation_api.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_provisioning_api.h"

#include "ble_mesh_example_init.h"
#include "board.h"

#define TAG "EXAMPLE"

#define VENDOR_CLIENT
#define CID_ESP 0x02E5

#define ESP_BLE_MESH_VND_MODEL_ID_CLIENT 0x0000
#define ESP_BLE_MESH_VND_MODEL_ID_SERVER 0x0001

#define ESP_BLE_MESH_VND_MODEL_OP_SEND ESP_BLE_MESH_MODEL_OP_3(0x00, CID_ESP)
#define ESP_BLE_MESH_VND_MODEL_OP_STATUS ESP_BLE_MESH_MODEL_OP_3(0x01, CID_ESP)

static uint8_t dev_uuid[ESP_BLE_MESH_OCTET16_LEN] = {0x32, 0x10};

static uint16_t stored_net_idx;
static uint16_t stored_app_idx;

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
#ifdef VENDOR_CLIENT
static esp_ble_mesh_client_t config_client;

static const esp_ble_mesh_client_op_pair_t vnd_op_pair[] = {
    {ESP_BLE_MESH_VND_MODEL_OP_SEND, ESP_BLE_MESH_VND_MODEL_OP_STATUS},
    {ESP_BLE_MESH_VND_MODEL_OP_STATUS, ESP_BLE_MESH_VND_MODEL_OP_SEND}, // 尚未确定是否有用
};

static esp_ble_mesh_client_t vendor_client = {
    .op_pair_size = ARRAY_SIZE(vnd_op_pair),
    .op_pair = vnd_op_pair,
};

/**
 * @description: 添加对什么op进行相应，只有这里添加了，ESP_BLE_MESH_MODEL_OPERATION_EVT中才会响应
 * @return {*}
 */
static esp_ble_mesh_model_op_t vnd_op_client[] = {
    ESP_BLE_MESH_MODEL_OP(ESP_BLE_MESH_VND_MODEL_OP_STATUS, 2),
    ESP_BLE_MESH_MODEL_OP(ESP_BLE_MESH_VND_MODEL_OP_SEND, 2),
    ESP_BLE_MESH_MODEL_OP_END,
};
#endif // VENDOR_CLIENT
static esp_ble_mesh_model_t root_models[] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&config_server),
    ESP_BLE_MESH_MODEL_CFG_CLI(&config_client),
};

#ifdef VENDOR_CLIENT
static esp_ble_mesh_model_t extend_model_0[] = {
    ESP_BLE_MESH_MODEL_CFG_CLI(&config_client),
};
#endif // VENDOR_CLIENT

static esp_ble_mesh_model_op_t vnd_op_server[] = {
    ESP_BLE_MESH_MODEL_OP(ESP_BLE_MESH_VND_MODEL_OP_SEND, 2),
    ESP_BLE_MESH_MODEL_OP(ESP_BLE_MESH_VND_MODEL_OP_STATUS, 2),
    ESP_BLE_MESH_MODEL_OP_END,
};

#ifdef VENDOR_CLIENT

static esp_ble_mesh_model_t vnd_models_client[] = {
    ESP_BLE_MESH_VENDOR_MODEL(CID_ESP, ESP_BLE_MESH_VND_MODEL_ID_CLIENT,
                              vnd_op_client, NULL, &vendor_client),
};
#endif // VENDOR_CLIENT

static esp_ble_mesh_model_t vnd_models_server[] = {
    ESP_BLE_MESH_VENDOR_MODEL(CID_ESP, ESP_BLE_MESH_VND_MODEL_ID_SERVER,
                              vnd_op_server, NULL, NULL),
};
#ifndef VENDOR_CLIENT
static esp_ble_mesh_elem_t elements[] = {
    ESP_BLE_MESH_ELEMENT(0, root_models, vnd_models),
};
#else
static esp_ble_mesh_elem_t elements[] = {
    ESP_BLE_MESH_ELEMENT(0, root_models, vnd_models_client),
    ESP_BLE_MESH_ELEMENT(0, ESP_BLE_MESH_MODEL_NONE, vnd_models_server),
    //    ESP_BLE_MESH_ELEMENT(0, ESP_BLE_MESH_MODEL_NONE, vnd_models),
};
#endif // VENDOR_CLIENT
static esp_ble_mesh_comp_t composition = {
    .cid = CID_ESP,
    .elements = elements,
    .element_count = ARRAY_SIZE(elements),
};

static esp_ble_mesh_prov_t provision = {
    .uuid = dev_uuid,
};

static void prov_complete(uint16_t net_idx, uint16_t addr, uint8_t flags, uint32_t iv_index)
{
    ESP_LOGI(TAG, "net_idx 0x%03x, addr 0x%04x", net_idx, addr);
    ESP_LOGI(TAG, "flags 0x%02x, iv_index 0x%08" PRIx32, flags, iv_index);
    board_led_operation(LED_G, LED_OFF);
}

static void example_ble_mesh_provisioning_cb(esp_ble_mesh_prov_cb_event_t event,
                                             esp_ble_mesh_prov_cb_param_t *param)
{
    switch (event) {
    case ESP_BLE_MESH_PROV_REGISTER_COMP_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_PROV_REGISTER_COMP_EVT, err_code %d", param->prov_register_comp.err_code);
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
        ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_RESET_EVT");
        break;
    case ESP_BLE_MESH_NODE_SET_UNPROV_DEV_NAME_COMP_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_SET_UNPROV_DEV_NAME_COMP_EVT, err_code %d", param->node_set_unprov_dev_name_comp.err_code);
        break;
    default:
        break;
    }
}

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
#if 1
            stored_net_idx = param->value.state_change.appkey_add.net_idx;
            stored_app_idx = param->value.state_change.appkey_add.app_idx;
#endif
            break;
        case ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND:
            ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND");
            ESP_LOGI(TAG, "elem_addr 0x%04x, app_idx 0x%04x, cid 0x%04x, mod_id 0x%04x",
                     param->value.state_change.mod_app_bind.element_addr,
                     param->value.state_change.mod_app_bind.app_idx,
                     param->value.state_change.mod_app_bind.company_id,
                     param->value.state_change.mod_app_bind.model_id);
            break;
        default:
            break;
        }
    }
}

static void example_ble_mesh_custom_model_cb(esp_ble_mesh_model_cb_event_t event,
                                             esp_ble_mesh_model_cb_param_t *param)
{
    esp_err_t err = ESP_OK;
#if 1
    esp_ble_mesh_msg_ctx_t ctx = {0};
    ctx.net_idx = stored_net_idx;
    ctx.app_idx = stored_net_idx;
    ctx.addr = 0xc001;
    ctx.send_ttl = 1;
    ctx.send_rel = 0;
#endif
    switch (event) {
    case ESP_BLE_MESH_MODEL_OPERATION_EVT:
        if (param->model_operation.opcode == ESP_BLE_MESH_VND_MODEL_OP_SEND) {
            ESP_LOGI(TAG, "Recv ESP_BLE_MESH_VND_MODEL_OP_SEND");
            printf("addr 0x%04x, recv_dst 0x%04x,msg: %d\n",
                   param->model_operation.ctx->addr,
                   param->model_operation.ctx->recv_dst,
                   (int)param->model_operation.msg);
#if 0
            err = esp_ble_mesh_server_model_send_msg(&vnd_models_server[0], param->model_operation.ctx, ESP_BLE_MESH_VND_MODEL_OP_STATUS,
                                                     sizeof(tid), (uint8_t *)&tid);
            if (err) {
                ESP_LOGE(TAG, "Failed to send message 0x%06x", ESP_BLE_MESH_VND_MODEL_OP_STATUS);
            }
#endif
        }
        if (param->model_operation.opcode == ESP_BLE_MESH_VND_MODEL_OP_STATUS) {
            ESP_LOGI(TAG, "Recv ESP_BLE_MESH_VND_MODEL_OP_STATUS");
            printf("addr 0x%04x, recv_dst 0x%04x,msg: %d\n",
                   param->model_operation.ctx->addr,
                   param->model_operation.ctx->recv_dst,
                   (int)param->model_operation.msg);
#if 0
            err = esp_ble_mesh_server_model_send_msg(&vnd_models_server[0], param->model_operation.ctx, ESP_BLE_MESH_VND_MODEL_OP_STATUS,
                                                     sizeof(tid), (uint8_t *)&tid);
            if (err) {
                ESP_LOGE(TAG, "Failed to send message 0x%06x", ESP_BLE_MESH_VND_MODEL_OP_STATUS);
            }
#endif
        }
        break;
    case ESP_BLE_MESH_MODEL_SEND_COMP_EVT:
        if (param->model_send_comp.err_code) {
            ESP_LOGE(TAG, "Failed to send message 0x%06" PRIx32, param->model_send_comp.opcode);
            break;
        }
        ESP_LOGI(TAG, "Send 0x%06" PRIx32, param->model_send_comp.opcode);
        break;
    default:
        break;
    }
}
#if 1
static void msg_send_task(void *arg)
{
    esp_ble_mesh_msg_ctx_t ctx = {0};
    uint32_t opcode;
    esp_err_t err;
    int vnd_tid = 0;
    ctx.net_idx = stored_net_idx;
    ctx.app_idx = stored_app_idx;
    ctx.addr = 0xc000;
    ctx.send_ttl = 3;
    ctx.send_rel = false;
    opcode = ESP_BLE_MESH_VND_MODEL_OP_SEND;
    while (1) {
        if (0 == false) {
            vnd_tid++;
        }
#if 1
        err = esp_ble_mesh_server_model_send_msg(&vnd_models_server[0], &ctx, opcode,
                                                 sizeof(vnd_tid), (uint8_t *)&vnd_tid);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send vendor message");
            printf("error: %d\n", err);
        }
        err = esp_ble_mesh_server_model_send_msg(&vnd_models_server[0], &ctx, ESP_BLE_MESH_VND_MODEL_OP_STATUS,
                                                 sizeof(vnd_tid), (uint8_t *)&vnd_tid);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send vendor message");
            printf("error: %d\n", err);
        }
#else
        err = esp_ble_mesh_client_model_send_msg(&vnd_models_client[0],
                                                 &ctx, opcode,
                                                 sizeof(vnd_tid), (uint8_t *)&vnd_tid,
                                                 0, true, ROLE_NODE);
        if (err) {
            ESP_LOGE(TAG, "Failed to send message 0x%06x", ESP_BLE_MESH_VND_MODEL_OP_SEND);
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
        err = esp_ble_mesh_client_model_send_msg(&vnd_models_client[0],
                                                 &ctx, ESP_BLE_MESH_VND_MODEL_OP_STATUS,
                                                 sizeof(vnd_tid), (uint8_t *)&vnd_tid,
                                                 0, true, ROLE_NODE);
        if (err) {
            ESP_LOGE(TAG, "Failed to send message 0x%06x", ESP_BLE_MESH_VND_MODEL_OP_STATUS);
        }
#endif
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
#endif

static esp_err_t ble_mesh_init(void)
{
    esp_err_t err;

    esp_ble_mesh_register_prov_callback(example_ble_mesh_provisioning_cb);
    esp_ble_mesh_register_config_server_callback(example_ble_mesh_config_server_cb);
    esp_ble_mesh_register_custom_model_callback(example_ble_mesh_custom_model_cb);

    err = esp_ble_mesh_init(&provision, &composition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize mesh stack");
        return err;
    }
#ifdef VENDOR_CLIENT
    err = esp_ble_mesh_client_model_init(&vnd_models_client[0]);
    if (err) {
        ESP_LOGE(TAG, "Failed to initialize vendor client");
        return err;
    }
#endif // VENDOR_CLIENT
    err = esp_ble_mesh_node_prov_enable(ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable mesh node");
        return err;
    }

    board_led_operation(LED_G, LED_ON);

    ESP_LOGI(TAG, "BLE Mesh Node initialized");

    return ESP_OK;
}

void app_main(void)
{
    esp_err_t err;

    ESP_LOGI(TAG, "Initializing...");

    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    board_init();

    err = bluetooth_init();
    if (err) {
        ESP_LOGE(TAG, "esp32_bluetooth_init failed (err %d)", err);
        return;
    }

    ble_mesh_get_dev_uuid(dev_uuid);

    /* Initialize the Bluetooth Mesh Subsystem */
    err = ble_mesh_init();
    if (err) {
        ESP_LOGE(TAG, "Bluetooth mesh init failed (err %d)", err);
    }

    xTaskCreate(msg_send_task, "msg_send_task", 1024 * 2, NULL, 5, NULL); // 接收任务
}
