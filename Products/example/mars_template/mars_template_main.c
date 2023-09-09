/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */
#include <stdio.h>
#include "iot_export_linkkit.h"
#include "iot_export_timer.h"
#include "cJSON.h"
#include "app_entry.h"
#include "aos/kv.h"

#include <dev_config.h>
#include "vendor.h"
#include "common/device_state_manger.h"

#include "mars_driver/mars_cloud.h"
#include "mars_driver/mars_uartmsg.h"
#include "mars_driver/mars_network.h"
#include "mars_driver/mars_ota.h"
#include "mars_devfunc/mars_devmgr.h"
#include "mars_devfunc/mars_factory.h"

#ifdef EN_COMBO_NET
#include "common/combo_net.h"
#include "breeze_export.h"
#endif

#ifdef CERTIFICATION_TEST_MODE
#include "certification/ct_ut.h"
#endif

#define USER_EXAMPLE_YIELD_TIMEOUT_MS (30)

// uart_port, tx, rx, baud_rate, data_width, parity, stop_bits, flow_control
// 默认串口
mars_uart_config_t g_mars_dev_uart[] = {
    // {1, 12, 1, 115200, M_DATAWIDTH_8BIT, M_NO_PARITY, M_STOPBITS_1, M_FLOWCONTROL_DISABLE},
    {1, 4, 3, 115200, M_DATAWIDTH_8BIT, M_NO_PARITY, M_STOPBITS_1, M_FLOWCONTROL_DISABLE},
};



void *example_malloc(size_t size)
{
    return HAL_Malloc(size);
}

void example_free(void *ptr)
{
    HAL_Free(ptr);
}

/** type:
 *
 * 0 - new firmware exist
 *
 */
// static int user_fota_event_handler(int type, const char *version)
// {
//     char buffer[128] = {0};
//     int buffer_length = 128;
//     mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();

//     if (type == 0) {
//         LOGI("mars", "New Firmware Version: %s", version);

//         IOT_Linkkit_Query(mars_template_ctx->master_devid, ITM_MSG_QUERY_FOTA_DATA, (unsigned char *)buffer, buffer_length);
//     }

//     return 0;
// }

static uint64_t user_update_sec(void)
{
    static uint64_t time_start_ms = 0;

    if (time_start_ms == 0) {
        time_start_ms = HAL_UptimeMs();
    }

    return (HAL_UptimeMs() - time_start_ms) / 1000;
}



#if defined (CLOUD_OFFLINE_RESET)
static int user_offline_reset_handler(void)
{
    LOGI("mars", "callback user_offline_reset_handler called.");
    vendor_device_unbind();
}
#endif

#if 0
void user_post_property_after_connected(void)
{
    int res = 0;
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();
    dev_status_t *device_status = &mars_template_ctx->status;
    char property_payload[128];

#ifdef TSL_FY_SUPPORT /* support old feiyan TSL */
    snprintf(property_payload, sizeof(property_payload), \
             "{\"%s\":%d, \"%s\":%d, \"%s\":%d}", "PowerSwitch", device_status->powerswitch, \
             "powerstate", device_status->powerswitch, "allPowerstate", device_status->all_powerstate);
#else
    snprintf(property_payload, sizeof(property_payload), \
             "{\"%s\":%d, \"%s\":%d}", "powerstate", device_status->powerswitch, \
             "allPowerstate", device_status->powerswitch, device_status->all_powerstate);
#endif

    res = IOT_Linkkit_Report(mars_template_ctx->master_devid, ITM_MSG_POST_PROPERTY,
            property_payload, strlen(property_payload));

    LOGI("mars", "Post Event Message ID: %d payload %s", res, property_payload);
}
#endif

static int user_master_dev_available(void)
{
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();

    if (mars_template_ctx->cloud_connected && mars_template_ctx->master_initialized) {
        return 1;
    }

    return 0;
}

/**
 * @brief 
 * @param {uint8_t} port
 * @param {void} *msg
 * @return {*}
 */        
extern void mars_otamodule_rsp(uint16_t seq,  uint8_t cmd, uint8_t *buf, uint16_t len);
extern void mars_uartmsg_ackseq_by_seq(uint16_t ack_seq);
int Mars_uartrecv_callback(uint8_t port, void *msg)
{
    uartmsg_que_t *recv_msg = (uartmsg_que_t *)msg;
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();
    int ret = 0;

    if (cmd_ack == recv_msg->cmd || cmd_nak == recv_msg->cmd )
    {
        mars_uartmsg_ackseq_by_seq(recv_msg->seq);
    }
    else if (recv_msg->cmd == cmd_ota_0B) //recv_msg->cmd == cmd_ota_0A
    {
        if (recv_msg->msg_buf[0] == prop_ota_response)
        {
            mars_uartmsg_ackseq_by_seq(recv_msg->seq);
            LOGI("mars", "开始删除对应的ota发送报文 seq=0x%04X", recv_msg->seq);
        }
        mars_otamodule_rsp(recv_msg->seq, recv_msg->cmd, recv_msg->msg_buf, recv_msg->len);
        // aos_post_event(EV_DEVFUNC, MARS_EVENT_OTAMODULE_STEP, 0);
    }
    else if (cmd_reqstatus == recv_msg->cmd)
    {
        //配网状态 烹饪助手状态 
    }
    else if (cmd_getack== recv_msg->cmd || cmd_event == recv_msg->cmd|| cmd_keypress == recv_msg->cmd)
    {
        if(mars_uart_prop_process(recv_msg))
        {
            //LOGI("mars", "mars_uart_prop_process: 1");
            char *property_payload = NULL;
            //LOGI("mars", "mars_uart_prop_process: 2");
            mars_property_data(NULL, &property_payload);
            //LOGI("mars", "mars_uart_prop_process: 3");
            if(property_payload)
            {
                //LOGI("mars", "mars_uart_prop_process: 4\r\n");
                ret = IOT_Linkkit_Report(mars_template_ctx->master_devid, ITM_MSG_POST_PROPERTY,
                                        (uint8_t *)property_payload, strlen(property_payload));
                //LOGI("mars", "生成属性数据 = %s", property_payload);
                if (ret == FAIL_RETURN)
                    LOGI("mars", "IOT_Linkkit_Report: 开始上报云端...... (失败!!! error)");
                else                
                    LOGI("mars", "IOT_Linkkit_Report: 开始上报云端...... (msg id = %d)", ret);
                cJSON_free(property_payload);
            }
            else
            {
                LOGI("mars", "本次数据未上报: mars_property_data() failed!!!");
            }
            property_payload = NULL;
        }
    }
    else
    {
        LOGI("mars", "未识别的命令字!!! (cmd=%d)", recv_msg->cmd);
    }

    if (recv_msg->msg_buf)
    {
        free(recv_msg->msg_buf);
        recv_msg->msg_buf = NULL;
    }

    return ret;
}

aos_timer_t g_get_device_status;
#define PERIOD_GET_DEVICE_STATUS_INTERVAL (1000 * 60)
static void timer_period_get_device_status_cb(void *arg1, void *arg2)
{
    // int state, len = sizeof(int);
    // int ret;

    // ret = aos_kv_get(KV_KEY_SWITCH_STATE, (void *)&state, &len);

//    if(ret != 0 || state != switch_state) {
//         LOG("KV DEVICE_STATUS Update!!!,ret=%d.\n",ret);
//         ret = aos_kv_set(KV_KEY_SWITCH_STATE, &switch_state, len, 0);
//         if (ret < 0) LOG("KV set Error: %d\r\n", __LINE__);
//     }
}


void Mars_dev_init(void)
{
    mars_cloud_init();
    int uart_sum = sizeof(g_mars_dev_uart)/sizeof(mars_uart_config_t);
    if (uart_sum != 0 && uart_sum <= 2){
        int i =0;
        Mars_uartmsg_init(g_mars_dev_uart[i].uart_port,
                        g_mars_dev_uart[i].tx, g_mars_dev_uart[i].rx,
                        g_mars_dev_uart[i].baud_rate, g_mars_dev_uart[i].data_width,
                        g_mars_dev_uart[i].stop_bits, g_mars_dev_uart[i].flow_control,
                        g_mars_dev_uart[i].parity, Mars_uartrecv_callback);
    }

    mars_devmgr_init();
    aos_timer_new(&g_get_device_status, timer_period_get_device_status_cb, NULL, PERIOD_GET_DEVICE_STATUS_INTERVAL, 1);
}



#define MAIN_LOOP_MS        200
int linkkit_main(void *paras)
{
    uint8_t                         time_temper_cnt = CA_TEMPER_TIME_MS/MAIN_LOOP_MS;
    int                             res = 0;
    iotx_linkkit_dev_meta_info_t    master_meta_info;
    mars_template_ctx_t             *mars_template_ctx = mars_dm_get_ctx();
    dev_status_t                    *device_status = &mars_template_ctx->status;

    LOGI("mars", "启动飞燕连接线程");
#if !defined(WIFI_PROVISION_ENABLED) || !defined(BUILD_AOS)
    set_device_meta_info();
#endif

    // memset(mars_template_ctx, 0, sizeof(mars_template_ctx_t));
    mars_template_ctx->master_devid = 0;

    /* Register Callback */
    Mars_network_init(Mars_property_set_callback, Mars_property_get_callback);

    memset(&master_meta_info, 0, sizeof(iotx_linkkit_dev_meta_info_t));
    HAL_GetProductKey(master_meta_info.product_key);
    HAL_GetDeviceName(master_meta_info.device_name);
    HAL_GetDeviceSecret(master_meta_info.device_secret);
    HAL_GetProductSecret(master_meta_info.product_secret);

    if ((0 == strlen(master_meta_info.product_key)) || (0 == strlen(master_meta_info.device_name))
            || (0 == strlen(master_meta_info.device_secret)) || (0 == strlen(master_meta_info.product_secret))) {
        LOGI("mars", "No device meta info found...\n");
        while (1) {
            aos_msleep(USER_EXAMPLE_YIELD_TIMEOUT_MS);
        }
    }

    /* Choose Login Method */
    int dynamic_register = 0;

#ifdef CERTIFICATION_TEST_MODE
#ifdef CT_PRODUCT_DYNAMIC_REGISTER_AND_USE_RAWDATA
    dynamic_register = 1;
#endif
#endif

    IOT_Ioctl(IOTX_IOCTL_SET_DYNAMIC_REGISTER, (void *)&dynamic_register);
#ifdef REPORT_UUID_ENABLE
    int uuid_enable = 1;
    IOT_Ioctl(IOTX_IOCTL_SET_UUID_ENABLED, (void *)&uuid_enable);
#endif

    /* Choose Whether You Need Post Property/Event Reply */
    int post_event_reply = 1;
    IOT_Ioctl(IOTX_IOCTL_RECV_EVENT_REPLY, (void *)&post_event_reply);

#ifdef CERTIFICATION_TEST_MODE
    ct_ut_init();
#endif

    /* Create Master Device Resources */
    do {
        mars_template_ctx->master_devid = IOT_Linkkit_Open(IOTX_LINKKIT_DEV_TYPE_MASTER, &master_meta_info);
        if (mars_template_ctx->master_devid < 0) {
            LOGI("mars", "IOT_Linkkit_Open Failed, retry after 5s...\n");
            HAL_SleepMs(5000);
        }
    } while (mars_template_ctx->master_devid < 0);
#ifdef AOS_TIMER_SERVICE
    int ret = timer_service_init(control_targets_list, NUM_OF_PROPERTYS,
            countdownlist_target_list, NUM_OF_COUNTDOWN_LIST_TARGET,
            localtimer_target_list, NUM_OF_LOCAL_TIMER_TARGET,
            timer_service_cb, num_of_tsl_type, NULL);
    if (ret == 0)
        ntp_update = true;
#endif
#ifdef AIOT_DEVICE_TIMER_ENABLE
    // aiot_device_timer_init(propertys_list, NUM_OF_TIMER_PROPERTYS, timer_service_cb);
#endif

#ifdef EN_COMBO_NET
    aiot_ais_report_awss_status(AIS_AWSS_STATUS_PROGRESS_REPORT, AIS_AWSS_PROGRESS_CONNECT_MQTT_START);
#endif

    /* Start Connect Aliyun Server */
    do {
        res = IOT_Linkkit_Connect(mars_template_ctx->master_devid);
        if (res < 0) {
            LOGI("mars", "IOT_Linkkit_Connect Failed, retry after 5s...\n");
            #ifdef EN_COMBO_NET
                aiot_ais_report_awss_status(AIS_AWSS_STATUS_PROGRESS_REPORT, AIS_AWSS_PROGRESS_CONNECT_MQTT_FAILED | (res & 0x00FF));
            #endif
            HAL_SleepMs(5000);
        }
    } while (res < 0);

    while (1) {
        IOT_Linkkit_Yield(USER_EXAMPLE_YIELD_TIMEOUT_MS);
#ifndef REPORT_MULTHREAD
        process_property_report();
#endif
        // extern void moduleota_loop(void);
        // moduleota_loop();

        // if (1 == device_status->RAuxiliarySwitch && 0 != device_status->RStoveStatus){
        //     if (0 == --time_temper_cnt){
        //         extern int mars_temper_report(void);
        //         mars_temper_report();
        //         time_temper_cnt = CA_TEMPER_TIME_MS/MAIN_LOOP_MS;
        //     }
        // }
    }

    IOT_Linkkit_Close(mars_template_ctx->master_devid);
    // IOT_DumpMemoryStats(IOT_LOG_NONE);
    // IOT_SetLogLevel(IOT_LOG_NONE);

    return 0;
}
