/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <aos/aos.h>
#include <aos/yloop.h>
#include <hal/wifi.h>
#include "netmgr.h"
#include "iot_export.h"
#include "iot_import.h"
#include "app_entry.h"
#include "aos/kv.h"
#include "vendor.h"
#include "dev_config.h"
#include "common/device_state_manger.h"
#include "mars_driver/mars_network.h"
#include "mars_driver/mars_uartmsg.h"
#include "mars_driver/mars_cloud.h"
#include "mars_devfunc/mars_devmgr.h"

//#include "lwip/ip_addr.h"
#include <hal/wifi.h>

//#include <hal/wifi_mgmr_ext.h>

#if defined(AOS_TIMER_SERVICE)||defined(AIOT_DEVICE_TIMER_ENABLE)
#include "iot_export_timer.h"
#endif
#ifdef CSP_LINUXHOST
#include <signal.h>
#endif

#include <k_api.h>

#if defined(OTA_ENABLED) && defined(BUILD_AOS)
// #include "ota_service.h"
#include "mars_driver/mars_ota.h"
#endif

#ifdef EN_COMBO_NET
#include "breeze_export.h"
#include "common/combo_net.h"
#endif


// static aos_task_t task_key_detect;
static aos_task_t task_msg_process;
static aos_task_t task_property_report;
static aos_task_t task_linkkit_reset;
static aos_task_t task_reboot_device;

static char linkkit_started = 0;
static char awss_dev_ap_started = 0;

extern int init_awss_flag(void);

void do_awss_active(void);

char* get_product_name(void)
{
	mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();
    if (strcmp(mars_template_ctx->product_key, PK_1) == 0)
    {
        return AD_NAME_1;
    }
    else if (strcmp(mars_template_ctx->product_key, PK_2) == 0)
    {
        return AD_NAME_2;
    }
    else
    {
        LOGE("mars", "get_product_name: 不匹配, 默认返回一体");
        return AD_NAME_1;
    }
}

void print_heap()
{
    extern k_mm_head *g_kmm_head;
    printf("\r\n");
    LOGI("mars", "===========================================================================");  
    LOGI("mars", "内存实时信息:  剩余内存=%d 已用内存=%d  (历史最大使用内存=%d)", g_kmm_head->free_size, g_kmm_head->used_size, g_kmm_head->maxused_size);
    LOGI("mars", "===========================================================================\r\n");
    //LOG("============free heap size =%d==========", free);
}

extern int mars_ble_awss_state;
static void wifi_service_event(input_event_t *event, void *priv_data)
{
    if (event->type != EV_WIFI) {
        return;
    }
    LOG("wifi_service_event(), code=%d", event->code);

    if (event->code == CODE_WIFI_ON_CONNECTED) 
    {
        LOGI("mars", "wifi连接进程: 路由器连接成功");
#ifdef EN_COMBO_NET
        aiot_ais_report_awss_status(AIS_AWSS_STATUS_PROGRESS_REPORT, AIS_AWSS_PROGRESS_CONNECT_AP_SUCCESS);
#endif
    } 
    else if (event->code == CODE_WIFI_ON_DISCONNECT) 
    {
        unsigned long reason_code = event->value;
        LOGI("mars", "wifi连接进程: 断开连接 (reason_code = %d)", reason_code);
        if (!is_awss_state()) 
        {
            mars_dm_get_ctx()->status.NetState = NET_STATE_NOT_CONNECTED;
            mars_store_netstatus();
        }

        if (mars_factory_status())//产测模式
        {            
            uint8_t send_buf[2] = {0};
            send_buf[0] = prop_FactoryResult;
            send_buf[1] = 0x01;
            Mars_uartmsg_send(cmd_store, uart_get_seq_mid(), send_buf, 2, 3);
            LOGI("mars", "产测: 产测失败!!!(连接热点失败)");
        }
#ifdef EN_COMBO_NET
        combo_set_ap_state(COMBO_AP_DISCONNECTED);
        aiot_ais_report_awss_status(AIS_AWSS_STATUS_PROGRESS_REPORT, AIS_AWSS_PROGRESS_CONNECT_AP_FAILED | reason_code);
#endif
    } 
    else if (event->code == CODE_WIFI_ON_CONNECT_FAILED) 
    {
        LOGI("mars", "wifi连接进程: 连接失败");
        if (!is_awss_state()) 
        {
            mars_dm_get_ctx()->status.NetState = NET_STATE_NOT_CONNECTED;
            mars_store_netstatus();
        }
#ifdef EN_COMBO_NET
        aiot_ais_report_awss_status(AIS_AWSS_STATUS_PROGRESS_REPORT, AIS_AWSS_PROGRESS_CONNECT_AP_FAILED);
#endif
    } 
    else if (event->code == CODE_WIFI_ON_GOT_IP) 
    {
        netmgr_ap_config_t config;
        memset(&config, 0, sizeof(netmgr_ap_config_t));
        netmgr_get_ap_config(&config);              
        char ipStr[16] = {0x00};
        netmgr_wifi_get_ip(ipStr);
        LOGI("mars", "wifi连接进程: 已分配ip (热点名称=%s 分配ip=%s)\r\n", config.ssid, ipStr);

        if (is_awss_state()) 
        {
            extern int xm_open_voice_file(char *file);
            LOGI("mars", "播报: 联网中，请稍后");
            xm_open_voice_file("113");
        }

        if (mars_factory_status())// 产测模式
		{            
            // uint8_t send_buf[2] = {0};
            // send_buf[0] = prop_FactoryResult;
            // send_buf[1] = 0x00;
            aos_post_event(EV_DEVFUNC, MARS_EVENT_FACTORY_LINKKEY, 0);
        }
#ifdef EN_COMBO_NET
        combo_set_ap_state(COMBO_AP_CONNECTED);
        aiot_ais_report_awss_status(AIS_AWSS_STATUS_PROGRESS_REPORT, AIS_AWSS_PROGRESS_GET_IP_SUCCESS);
#endif
    }

    if (event->code != CODE_WIFI_ON_GOT_IP) {
        return;
    }

    netmgr_ap_config_t config;
    memset(&config, 0, sizeof(netmgr_ap_config_t));
    netmgr_get_ap_config(&config);
    //LOG("wifi_service_event config.ssid %s", config.ssid);
    if (strcmp(config.ssid, "adha") == 0 || strcmp(config.ssid, "aha") == 0) {
        // clear_wifi_ssid();
        return;
    }
    set_net_state(GOT_AP_SSID);
#ifdef EN_COMBO_NET
    combo_ap_conn_notify();
#endif

    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();
    if (!linkkit_started && !mars_factory_status()) {
#ifdef CONFIG_PRINT_HEAP
        print_heap();
#endif
#if (defined (TG7100CEVB))        
        aos_task_new("linkkit", (void (*)(void *))linkkit_main, NULL, 1024 * 8);
#else
        aos_task_new("linkkit", (void (*)(void *))linkkit_main, NULL, 1024 * 6);
#endif
        linkkit_started = 1;
    }
}

static void cloud_service_event(input_event_t *event, void *priv_data)
{
    if (event->type != EV_YUNIO) {
        return;
    }

    LOG("cloud_service_event %d", event->code);

    if (event->code == CODE_YUNIO_ON_CONNECTED) {
        LOG("user sub and pub here");
        return;
    }

    if (event->code == CODE_YUNIO_ON_DISCONNECTED) {
    }
}

/*
 * Note:
 * the linkkit_event_monitor must not block and should run to complete fast
 * if user wants to do complex operation with much time,
 * user should post one task to do this, not implement complex operation in
 * linkkit_event_monitor
 */
extern int mars_ble_awss_state;
static void linkkit_event_monitor(int event)
{
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();
    switch (event) {
        case IOTX_AWSS_START: // AWSS start without enbale, just supports device discover
            // operate led to indicate user
            LOGW("mars", "linkkit_event_monitor: IOTX_AWSS_START");
            break;
        case IOTX_AWSS_ENABLE: // AWSS enable, AWSS doesn't parse awss packet until AWSS is enabled.
            LOGW("mars", "linkkit_event_monitor: IOTX_AWSS_ENABLE");
            // operate led to indicate user
            break;
        case IOTX_AWSS_LOCK_CHAN: // AWSS lock channel(Got AWSS sync packet)
            LOGW("mars", "linkkit_event_monitor: IOTX_AWSS_LOCK_CHAN");
            // operate led to indicate user
            break;
        case IOTX_AWSS_PASSWD_ERR: // AWSS decrypt passwd error
            LOGW("mars", "linkkit_event_monitor: IOTX_AWSS_PASSWD_ERR");
            // operate led to indicate user
            break;
        case IOTX_AWSS_GOT_SSID_PASSWD:
            LOGW("mars", "linkkit_event_monitor: IOTX_AWSS_GOT_SSID_PASSWD");
            // operate led to indicate user
            set_net_state(GOT_AP_SSID);
            break;
        case IOTX_AWSS_CONNECT_ADHA: // AWSS try to connnect adha (device
            // discover, router solution)
            LOGW("mars", "linkkit_event_monitor: IOTX_AWSS_CONNECT_ADHA");
            // operate led to indicate user
            break;
        case IOTX_AWSS_CONNECT_ADHA_FAIL: // AWSS fails to connect adha
            LOGW("mars", "linkkit_event_monitor: IOTX_AWSS_CONNECT_ADHA_FAIL");
            // operate led to indicate user
            break;
        case IOTX_AWSS_CONNECT_AHA: // AWSS try to connect aha (AP solution)
            LOGW("mars", "linkkit_event_monitor: IOTX_AWSS_CONNECT_AHA");
            // operate led to indicate user
            break;
        case IOTX_AWSS_CONNECT_AHA_FAIL: // AWSS fails to connect aha
            LOGW("mars", "linkkit_event_monitor: IOTX_AWSS_CONNECT_AHA_FAIL");
            // operate led to indicate user
            break;
        case IOTX_AWSS_SETUP_NOTIFY: // AWSS sends out device setup information
            // (AP and router solution)
            LOGW("mars", "linkkit_event_monitor: IOTX_AWSS_SETUP_NOTIFY");
            // operate led to indicate user
            break;
        case IOTX_AWSS_CONNECT_ROUTER: // AWSS try to connect destination router
            LOGW("mars", "linkkit_event_monitor: IOTX_AWSS_CONNECT_ROUTER");
            // operate led to indicate user
            break;
        case IOTX_AWSS_CONNECT_ROUTER_FAIL: // AWSS fails to connect destination
            // router.
            LOGW("mars", "linkkit_event_monitor: IOTX_AWSS_CONNECT_ROUTER_FAIL");
            set_net_state(CONNECT_AP_FAILED);
            // operate led to indicate user
            break;
        case IOTX_AWSS_GOT_IP: // AWSS connects destination successfully and got
            // ip address
            LOGW("mars", "linkkit_event_monitor: IOTX_AWSS_GOT_IP");
            // operate led to indicate user
            break;
        case IOTX_AWSS_SUC_NOTIFY: // AWSS sends out success notify (AWSS
            // sucess)
            LOGW("mars", "linkkit_event_monitor: IOTX_AWSS_SUC_NOTIFY");
            // operate led to indicate user
            break;
        case IOTX_AWSS_BIND_NOTIFY: // AWSS sends out bind notify information to
            // support bind between user and device
            LOGW("mars", "linkkit_event_monitor: IOTX_AWSS_BIND_NOTIFY");
            // operate led to indicate user
            // mars_weatherpost_init();
            break;
        case IOTX_AWSS_ENABLE_TIMEOUT: // AWSS enable timeout            
            LOGW("mars", "linkkit_event_monitor: IOTX_AWSS_ENALBE_TIMEOUT");
            break;
        case IOTX_CONN_CLOUD: // Device try to connect cloud
            LOGW("mars", "linkkit_event_monitor: IOTX_CONN_CLOUD");            
            break;
        case IOTX_CONN_CLOUD_FAIL: // Device fails to connect cloud, refer to
            LOGW("mars", "linkkit_event_monitor: IOTX_CONN_CLOUD_FAIL");
            mars_template_ctx->cloud_connected = 0;
            combo_set_cloud_state(0);
            set_net_state(CONNECT_CLOUD_FAILED); 
            if (!is_awss_state()) 
            {
                mars_dm_get_ctx()->status.NetState = NET_STATE_NOT_CONNECTED;
                mars_store_netstatus();
            }
            break;

        case IOTX_CONN_CLOUD_SUC: // Device connects cloud successfully
            LOGW("mars", "linkkit_event_monitor: IOTX_CONN_CLOUD_SUC");
            mars_template_ctx->cloud_connected = 1;
            combo_set_cloud_state(1);
            set_net_state(CONNECT_CLOUD_SUCCESS);
            if (!is_awss_state())
            {
                mars_dm_get_ctx()->status.NetState = NET_STATE_CONNECTED;
                mars_store_netstatus();
            }
            break;

        case IOTX_RESET: // Linkkit reset success (just got reset response from
            LOGW("mars", "linkkit_event_monitor: IOTX_RESET");
            mars_template_ctx->bind_notified = 0;
            break;

        case IOTX_CONN_REPORT_TOKEN_SUC:
            LOGW("mars", "linkkit_event_monitor: IOTX_CONN_REPORT_TOKEN_SUC");
            combo_token_report_notify();
            mars_dm_get_ctx()->status.TokenRptFlag = 1;
            mars_dm_get_ctx()->status.NetState = NET_STATE_CONNECTED;
            mars_store_netstatus();
            timer_func_awss_finish(NULL, NULL);
            break;

        default:
            break;
    }
}

#ifdef AWSS_BATCH_DEVAP_ENABLE
#define DEV_AP_ZCONFIG_TIMEOUT_MS  120000 // (ms)
extern void awss_set_config_press(uint8_t press);
extern uint8_t awss_get_config_press(void);
extern void zconfig_80211_frame_filter_set(uint8_t filter, uint8_t fix_channel);
void do_awss_dev_ap();

static aos_timer_t dev_ap_zconfig_timeout_timer;
static uint8_t g_dev_ap_zconfig_timer = 0; // this timer create once and can restart
static uint8_t g_dev_ap_zconfig_run = 0;

static void timer_func_devap_zconfig_timeout(void *arg1, void *arg2)
{
    LOG("%s run\n", __func__);

    if (awss_get_config_press()) {
        // still in zero wifi provision stage, should stop and switch to dev ap
        do_awss_dev_ap();
    } else {
        // zero wifi provision finished
    }

    awss_set_config_press(0);
    zconfig_80211_frame_filter_set(0xFF, 0xFF);
    g_dev_ap_zconfig_run = 0;
    aos_timer_stop(&dev_ap_zconfig_timeout_timer);
}

static void awss_dev_ap_switch_to_zeroconfig(void *p)
{
    LOG("%s run\n", __func__);
    // Stop dev ap wifi provision
    awss_dev_ap_stop();
    // Start and enable zero wifi provision
    iotx_event_regist_cb(linkkit_event_monitor);
    awss_set_config_press(1);

    // Start timer to count duration time of zero provision timeout
    if (!g_dev_ap_zconfig_timer) {
        aos_timer_new(&dev_ap_zconfig_timeout_timer, timer_func_devap_zconfig_timeout, NULL, DEV_AP_ZCONFIG_TIMEOUT_MS, 0);
        g_dev_ap_zconfig_timer = 1;
    }
    aos_timer_start(&dev_ap_zconfig_timeout_timer);

    // This will hold thread, when awss is going
    netmgr_start(true);

    LOG("%s exit\n", __func__);
    aos_task_exit(0);
}

int awss_dev_ap_modeswitch_cb(uint8_t awss_new_mode, uint8_t new_mode_timeout, uint8_t fix_channel)
{
    if ((awss_new_mode == 0) && !g_dev_ap_zconfig_run) {
        g_dev_ap_zconfig_run = 1;
        // Only receive zero provision packets
        zconfig_80211_frame_filter_set(0x00, fix_channel);
        LOG("switch to awssmode %d, mode_timeout %d, chan %d\n", 0x00, new_mode_timeout, fix_channel);
        // switch to zero config
        aos_task_new("devap_to_zeroconfig", awss_dev_ap_switch_to_zeroconfig, NULL, 2048);
    }
}
#endif

static void awss_close_dev_ap(void *p)
{
#ifdef AWSS_SUPPORT_DEV_AP
    awss_dev_ap_stop();
#endif
    awss_dev_ap_started = 0;
    aos_task_exit(0);
}

void awss_open_dev_ap(void *p)
{
    iotx_event_regist_cb(linkkit_event_monitor);
    /*if (netmgr_start(false) != 0) */{
        awss_dev_ap_started = 1;
        //aos_msleep(2000);
#ifdef AWSS_BATCH_DEVAP_ENABLE
        awss_dev_ap_reg_modeswit_cb(awss_dev_ap_modeswitch_cb);
#endif
#ifdef AWSS_SUPPORT_DEV_AP
        awss_dev_ap_start();
#endif
    }
    aos_task_exit(0);
}

void stop_netmgr(void *p)
{
    awss_stop();
    aos_task_exit(0);
}

void start_netmgr(void *p)
{
    /* wait for dev_ap mode stop done */
    do {
        aos_msleep(100);
    } while (awss_dev_ap_started);
    iotx_event_regist_cb(linkkit_event_monitor);
    netmgr_start(true);
    aos_task_exit(0);
}

void do_awss_active(void)
{
    LOG("do_awss_active");
#ifdef WIFI_PROVISION_ENABLED
    extern int awss_config_press();
    awss_config_press();
#endif
}

#ifdef EN_COMBO_NET

void combo_open(void)
{
    combo_net_init();
}

void ble_awss_open(void *p)
{
    iotx_event_regist_cb(linkkit_event_monitor);
    combo_set_awss_state(1);
    aos_task_exit(0);
}

static void ble_awss_close(void *p)
{
    combo_set_awss_state(0);
    aos_task_exit(0);
}

void do_ble_awss()
{
    aos_task_new("ble_awss_open", ble_awss_open, NULL, 2048);
}
#endif

void do_awss_dev_ap()
{
    // Enter dev_ap awss mode
    aos_task_new("netmgr_stop", stop_netmgr, NULL, 4096);
    aos_task_new("dap_open", awss_open_dev_ap, NULL, 4096);
}

void do_awss()
{
    // Enter smart_config awss mode
    aos_task_new("dap_close", awss_close_dev_ap, NULL, 4096);
    aos_task_new("netmgr_start", start_netmgr, NULL, 5120);
}

void linkkit_reset(void *p)
{
    aos_msleep(2000);
#ifdef AOS_TIMER_SERVICE
    timer_service_clear();
#endif
#ifdef AIOT_DEVICE_TIMER_ENABLE
    aiot_device_timer_clear();
#endif
    iotx_sdk_reset_local();
    netmgr_clear_ap_config();
#ifdef EN_COMBO_NET
    breeze_clear_bind_info();
#endif
    HAL_Reboot();
    aos_task_exit(0);
}

extern int iotx_sdk_reset(iotx_vendor_dev_reset_type_t *reset_type);
iotx_vendor_dev_reset_type_t reset_type = IOTX_VENDOR_DEV_RESET_TYPE_UNBIND_ONLY;
void do_awss_reset(void)
{
#ifdef WIFI_PROVISION_ENABLED
    aos_task_new("reset", (void (*)(void *))iotx_sdk_reset, &reset_type, 6144);  // stack taken by iTLS is more than taken by TLS.
#endif
    aos_task_new_ext(&task_linkkit_reset, "reset task", linkkit_reset, NULL, 1024, 0);
}

void clear_awss_info()
{
    iotx_sdk_reset_local();
    netmgr_clear_ap_config();
    breeze_clear_bind_info();
}

void reboot_device(void *p)
{
    aos_msleep(500);
    HAL_Reboot();
    aos_task_exit(0);
}

void do_awss_reboot(void)
{
#ifdef AOS_TIMER_SERVICE
    timer_service_clear();
#endif

#ifdef AIOT_DEVICE_TIMER_ENABLE
    aiot_device_timer_clear();
#endif

    iotx_sdk_reset_local();
    netmgr_clear_ap_config();
#ifdef EN_COMBO_NET
    breeze_clear_bind_info();
#endif

    unsigned char awss_flag = 1;
    if (aos_kv_set("awss_flag", &awss_flag, 1, 1) != 0)
    {
        LOGE("mars", "错误: 待配网标志写入失败!!! fail");
        aos_msleep(1000);
        aos_kv_set("awss_flag", &awss_flag, 1, 1);
    }
    aos_msleep(500);
    HAL_Reboot();
    //aos_task_new_ext(&task_reboot_device, "reboot task", reboot_device, NULL, 1024, 0);
}

// void linkkit_key_process(input_event_t *eventinfo, void *priv_data)
// {
//     if (eventinfo->type != EV_KEY) {
//         return;
//     }
//     LOG("awss config press %d\n", eventinfo->value);

//     if (eventinfo->code == CODE_BOOT) {
//         if (eventinfo->value == VALUE_KEY_CLICK) {
//             do_awss_active();
//         } else if (eventinfo->value == VALUE_KEY_LTCLICK) {
//             do_awss_reset();
//         }
//     }
// }

#ifdef MANUFACT_AP_FIND_ENABLE
void manufact_ap_find_process(int result)
{
    // Informed manufact ap found or not.
    // If manufact ap found, lower layer will auto connect the manufact ap
    // IF manufact ap not found, lower layer will enter normal awss state
    if (result == 0) {
        LOG("%s ap found.\n", __func__);
    } else {
        LOG("%s ap not found.\n", __func__);
    }
}
#endif

#ifdef CONFIG_AOS_CLI
static void handle_reset_cmd(char *pwbuf, int blen, int argc, char **argv)
{
    aos_schedule_call((aos_call_t)do_awss_reset, NULL);
}

static void handle_active_cmd(char *pwbuf, int blen, int argc, char **argv)
{
    aos_schedule_call((aos_call_t)do_awss_active, NULL);
}

static void handle_dev_ap_cmd(char *pwbuf, int blen, int argc, char **argv)
{
    aos_schedule_call((aos_call_t)do_awss_dev_ap, NULL);
}

#ifdef EN_COMBO_NET
static void handle_ble_awss_cmd(char *pwbuf, int blen, int argc, char **argv)
{
    LOGI("mars", "handle_ble_awss_cmd: 进入蓝牙配网状态\r\n");
    aos_schedule_call((aos_call_t)do_ble_awss, NULL);
}
#endif

static void handle_linkkey_cmd(char *pwbuf, int blen, int argc, char **argv)
{
    if (argc == 1) {
        int len = 0;
        char product_key[PRODUCT_KEY_LEN + 1] = { 0 };
        char product_secret[PRODUCT_SECRET_LEN + 1] = { 0 };
        char device_name[DEVICE_NAME_LEN + 1] = { 0 };
        char device_secret[DEVICE_SECRET_LEN + 1] = { 0 };
        char pidStr[9] = { 0 };

        len = PRODUCT_KEY_LEN + 1;
        aos_kv_get("linkkit_product_key", product_key, &len);

        len = PRODUCT_SECRET_LEN + 1;
        aos_kv_get("linkkit_product_secret", product_secret, &len);

        len = DEVICE_NAME_LEN + 1;
        aos_kv_get("linkkit_device_name", device_name, &len);

        len = DEVICE_SECRET_LEN + 1;
        aos_kv_get("linkkit_device_secret", device_secret, &len);

        aos_cli_printf("Product Key=%s.\r\n", product_key);
        aos_cli_printf("Device Name=%s.\r\n", device_name);
        aos_cli_printf("Device Secret=%s.\r\n", device_secret);
        aos_cli_printf("Product Secret=%s.\r\n", product_secret);
        len = sizeof(pidStr);
        if (aos_kv_get("linkkit_product_id", pidStr, &len) == 0) {
            aos_cli_printf("Product Id=%d.\r\n", atoi(pidStr));
        }
    } else if (argc == 5 || argc == 6) {
        aos_kv_set("linkkit_product_key", argv[1], strlen(argv[1]) + 1, 1);
        aos_kv_set("linkkit_device_name", argv[2], strlen(argv[2]) + 1, 1);
        aos_kv_set("linkkit_device_secret", argv[3], strlen(argv[3]) + 1, 1);
        aos_kv_set("linkkit_product_secret", argv[4], strlen(argv[4]) + 1, 1);
        if (argc == 6)
            aos_kv_set("linkkit_product_id", argv[5], strlen(argv[5]) + 1, 1);
        aos_cli_printf("Done");
    } else {
        aos_cli_printf("Error: %d\r\n", __LINE__);
        return;
    }
}

static void handle_awss_cmd(char *pwbuf, int blen, int argc, char **argv)
{
    aos_schedule_call((aos_call_t)do_awss, NULL);
}

static struct cli_command resetcmd = {
    .name = "reset",
    .help = "factory reset",
    .function = handle_reset_cmd
};

static struct cli_command awss_enable_cmd = {
    .name = "active_awss",
    .help = "active_awss [start]",
    .function = handle_active_cmd
};

static struct cli_command awss_dev_ap_cmd = {
    .name = "dev_ap",
    .help = "awss_dev_ap [start]",
    .function = handle_dev_ap_cmd
};

static struct cli_command awss_cmd = {
    .name = "awss",
    .help = "awss [start]",
    .function = handle_awss_cmd
};

#ifdef EN_COMBO_NET
static struct cli_command awss_ble_cmd = {
    .name = "ble_awss",
    .help = "ble_awss [start]",
    .function = handle_ble_awss_cmd
};

static void handle_ble_status_report_cmd(char *pwbuf, int blen, int argc, char **argv) {

    if (argc == 3) {
        aiot_ais_report_awss_status(atoi(argv[1]), atoi(argv[2]));
    }
}

static struct cli_command ble_status_report_cmd = {
    .name = "ble_report",
    .help = "ble_report [status] [code]",
    .function = handle_ble_status_report_cmd
};
#endif

static struct cli_command linkkeycmd = {
    .name = "linkkey",
    .help = "set/get linkkit keys. linkkey [<Product Key> <Device Name> <Device Secret> <Product Secret>]",
    .function = handle_linkkey_cmd
};

#endif


static void handle_mfg_cmd(char *pwbuf, int blen, int argc, char **argv)
{
    uart0_rbuf_init(115200);
}

static uint8_t hex(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'Z') {
        return c - 'A' + 10;
    }
    return 0;
}

static void macstr2bin(const char *macstr, uint8_t *mac, int len)
{
    int i;
    for (i = 0; i < len; i++) {
        mac[i] = hex(macstr[3 * i]) << 4;
        mac[i] |= hex(macstr[3 * i + 1]);
    }
}

static void handle_wem_cmd(char *pwbuf, int blen, int argc, char **argv)
{
    uint8_t mac[6];

    aos_cli_printf("[mfg fw]%s OK\r\n", argv[0]);
    
    if (argc == 1) {
        extern uint8_t mfg_media_is_macaddr_slot_empty(uint8_t reload);
        mfg_media_is_macaddr_slot_empty(1);

        macstr2bin(argv[0]+3, mac, 6);
        aos_cli_printf("MAC address cmd:%02x %02x %02x %02x %02x %02x\r\n",
                       mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        
        extern int8_t mfg_media_write_macaddr_pre_with_lock(uint8_t mac[6],uint8_t program);
        mfg_media_write_macaddr_pre_with_lock(mac, 0);
        // hal_wifi_set_mac_addr(NULL, mac);
    } else {
        aos_cli_printf("invalid cmd\r\n");
    }
}

static void handle_test_cmd(char *pwbuf, int blen, int argc, char **argv)
{
    if (argc == 1) {
        aos_cli_printf("OK\r\n");
    } else {
        aos_cli_printf("invalid cmd\r\n");
    }
}

static void handle_sem_cmd(char *pwbuf, int blen, int argc, char **argv)
{
    uint8_t mac[6];

    if (argc == 1) {
        extern void mfg_media_write_macaddr_with_lock(void);
        mfg_media_write_macaddr_with_lock();
        aos_cli_printf("[mfg fw] SEM OK Save MAC address OK\r\n");
    } else {
        aos_cli_printf("invalid cmd\r\n");
    }
}

static void handle_rem_cmd(char *pwbuf, int blen, int argc, char **argv)
{
    uint8_t mac[6];
    aos_cli_printf("[mfg fw] REM OK");
    extern int bl_efuse_read_mac_factory(uint8_t mac[6]);
    bl_efuse_read_mac_factory(mac);
    aos_cli_printf("MAC:%02x:%02x:%02x:%02x:%02x:%02x\r\n",
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static struct cli_command mfg_cli_cmd[] = 
{
 // { "mfg",  "into mfg mode",  handle_mfg_cmd },
    { "H",     "test",          handle_test_cmd  },
    { "WEM",   "write mac",     handle_wem_cmd   },
    { "SEM",   "save mac",      handle_sem_cmd   },
    { "REM",   "read mac",      handle_rem_cmd   },
    { "Reset", "reset",         handle_reset_cmd },
};

#ifdef CONFIG_PRINT_HEAP
static void duration_work(void *p)
{
    print_heap();
    //aos_post_delayed_action(5000, duration_work, NULL);
}
#endif


static int mqtt_connected_event_handler(void)
{
    int ret = 0;
#if defined(OTA_ENABLED) && defined(BUILD_AOS)
    ret = mars_ota_mqtt_connected();

#endif
    return ret;
}

static void show_firmware_version(void)
{
    LOGI("mars", "--------------固件信息--------------");
    LOGI("mars", "App版本: %s", aos_get_app_version());
    LOGI("mars", "Aos版本: %s", aos_get_kernel_version());
    LOGI("mars", "Sdk版本: %s (LinkKit)", LINKKIT_VERSION);
    LOGI("mars", "编译时间: %04d-%02d-%02d %02d:%02d:%02d", BUILD_YEAR, BUILD_MONTH, BUILD_DAY, BUILD_HOUR, BUILD_MIN, BUILD_SEC);    
    LOGI("mars", "AppName: %s,  Platform: %s", APP_NAME, PLATFORM);
    LOGI("mars", "Host: %s",   COMPILE_HOST);
    LOGI("mars", "Branch: %s", GIT_BRANCH);
    LOGI("mars", "Hash: %s",   GIT_HASH);
    LOGI("mars", "Region env: %s\r\n", REGION_ENV_STRING);
}

#if 1//(defined (TG7100CEVB))
void media_to_kv(void)
{
    char product_key[PRODUCT_KEY_LEN + 1] = { 0 };
    char *p_product_key = NULL;
    char product_secret[PRODUCT_SECRET_LEN + 1] = { 0 };
    char *p_product_secret = NULL;
    char device_name[DEVICE_NAME_LEN + 1] = { 0 };
    char *p_device_name = NULL;
    char device_secret[DEVICE_SECRET_LEN + 1] = { 0 };
    char *p_device_secret = NULL;
    char pidStr[9] = { 0 };
    char *p_pidStr = NULL;
    int len;

    int res;

    /* check media valid, and update p */
    res = ali_factory_media_get(
                &p_product_key,
                &p_product_secret,
                &p_device_name,
                &p_device_secret,
                &p_pidStr);
    if (0 != res) {
        printf("ali_factory_media_get res = %d\r\n", res);
        return;
    }

    /* compare kv media */
    len = sizeof(product_key);
    aos_kv_get("linkkit_product_key", product_key, &len);
    len = sizeof(product_secret);
    aos_kv_get("linkkit_product_secret", product_secret, &len);
    len = sizeof(device_name);
    aos_kv_get("linkkit_device_name", device_name, &len);
    len = sizeof(device_secret);
    aos_kv_get("linkkit_device_secret", device_secret, &len);
    len = sizeof(pidStr);
    aos_kv_get("linkkit_product_id", pidStr, &len);

    if (p_product_key) {
        if (0 != memcmp(product_key, p_product_key, strlen(p_product_key))) {
            printf("memcmp p_product_key different. set kv: %s\r\n", p_product_key);
            aos_kv_set("linkkit_product_key", p_product_key, strlen(p_product_key), 1);
        }
    }
    if (p_product_secret) {
        if (0 != memcmp(product_secret, p_product_secret, strlen(p_product_secret))) {
            printf("memcmp p_product_secret different. set kv: %s\r\n", p_product_secret);
            aos_kv_set("linkkit_product_secret", p_product_secret, strlen(p_product_secret), 1);
        }
    }
    if (p_device_name) {
        if (0 != memcmp(device_name, p_device_name, strlen(p_device_name))) {
            printf("memcmp p_device_name different. set kv: %s\r\n", p_device_name);
            aos_kv_set("linkkit_device_name", p_device_name, strlen(p_device_name), 1);
        }
    }
    if (p_device_secret) {
        if (0 != memcmp(device_secret, p_device_secret, strlen(p_device_secret))) {
            printf("memcmp p_device_secret different. set kv: %s\r\n", p_device_secret);
            aos_kv_set("linkkit_device_secret", p_device_secret, strlen(p_device_secret), 1);
        }
    }
    if (p_pidStr) {
        if (0 != memcmp(pidStr, p_pidStr, strlen(p_pidStr))) {
            printf("memcmp p_pidStr different. set kv: %s\r\n", p_pidStr);
            aos_kv_set("linkkit_product_id", p_pidStr, strlen(p_pidStr), 1);
        }
    }

    // uint8_t mac[6] = {0};
    // char mac_str[20] = {0};
    // if (0 == hal_wifi_get_mac_addr(NULL, mac)){
    //     sprintf(mac_str, "%02x%02x%02x%02x%02x%02x",
    //         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    //     memcpy(mars_dm_get_ctx()->macStr, mac_str, strlen(mac_str));
    // }

}
#endif


int mars_awss_event_callback(input_event_t *event_id, void *param)
{
	switch(event_id->code)
	{
        case MARS_EVENT_AWSS_START:
        {
            if (!is_awss_state())
            {
                LOGW("mars", "进入配网状态\r\n)");
                //reset_awss_state();
                //Mars_start_wifi_awss(1);
                Mars_start_wifi_awss_reset();
            }
            break;
        }

        case MARS_EVENT_AWSS_EXIT:
        {
            if (is_awss_state())
            {
                LOGW("mars", "退出配网状态\r\n)");
                mars_ble_awss_state = 0x00; 
                mars_store_netstatus();
                set_net_state(AWSS_NOT_START);
                device_stop_awss();
            }
            break;
        }

        default:
            break;
	}

	return 0;
}

void init_mars_event(void)
{
    aos_register_event_filter(EV_TG7100AWSS, mars_awss_event_callback, NULL);
}

void init_meat_info(void)
{
    set_device_meta_info();
    if (memcmp(mars_dm_get_ctx()->product_key,    PK_1,  strlen(PK_1))  == 0 && 
        memcmp(mars_dm_get_ctx()->product_secret, PS_1,  strlen(PS_1))  == 0  )
    {
        LOGI("mars", "四元组类型: %s一体", PRO_NAME);
    }
    else if (memcmp(mars_dm_get_ctx()->product_key,    PK_2,  strlen(PK_2))  == 0 && 
             memcmp(mars_dm_get_ctx()->product_secret, PS_2,  strlen(PS_2))  == 0 )
    {
        LOGI("mars", "四元组类型: %s两体", PRO_NAME);
    }
    else 
    {
        LOGE("mars", "四元组类型: 未烧录四元组!!!");
    }

    uint8_t mac[6] = {0};
    if (hal_wifi_get_mac_addr(NULL, mac) == 0)
    {
        snprintf(mars_dm_get_ctx()->macStr, sizeof(mars_dm_get_ctx()->macStr), "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        LOGI("mars", "设备wifi mac = %s", mars_dm_get_ctx()->macStr);
    }
    if (memcmp(mars_dm_get_ctx()->macStr, mars_dm_get_ctx()->device_name, 12) != 0)
    {
        LOGE("mars", "错误: dn与mac不匹配,请检查dn是否与mac匹配!!! (dn=%s  mac=%s)", mars_dm_get_ctx()->device_name, mars_dm_get_ctx()->macStr);
    }
}

int application_start(int argc, char **argv)
{
#if (defined (TG7100CEVB))
    media_to_kv();
#endif

#ifdef CONFIG_PRINT_HEAP
    print_heap();
    //aos_post_delayed_action(5000, duration_work, NULL);
#endif

#ifdef CSP_LINUXHOST
    signal(SIGPIPE, SIG_IGN);
#endif

#ifdef WITH_SAL
    sal_init();
#endif

#ifdef MDAL_MAL_ICA_TEST
    HAL_MDAL_MAL_Init();
#endif

#ifdef DEFAULT_LOG_LEVEL_DEBUG
    IOT_SetLogLevel(IOT_LOG_DEBUG);
#else
    IOT_SetLogLevel(IOT_LOG_INFO);
#endif

    show_firmware_version();
        
    init_meat_info(); /* Must call init_meat_info before netmgr_init */

    netmgr_init();

    // vendor_product_init();

    dev_diagnosis_module_init();
    
#ifdef DEV_OFFLINE_OTA_ENABLE
    mars_ota_init();
#endif

#ifdef MANUFACT_AP_FIND_ENABLE
    // netmgr_manuap_info_set("TEST_AP", "TEST_PASSWORD", manufact_ap_find_process);
#endif

    // aos_register_event_filter(EV_KEY, linkkit_key_process, NULL);
    aos_register_event_filter(EV_WIFI, wifi_service_event, NULL);
    aos_register_event_filter(EV_YUNIO, cloud_service_event, NULL);
    IOT_RegisterCallback(ITE_MQTT_CONNECT_SUCC,mqtt_connected_event_handler);

#ifdef CONFIG_AOS_CLI
    aos_cli_register_command(&resetcmd);
    aos_cli_register_command(&awss_enable_cmd);
    aos_cli_register_command(&awss_dev_ap_cmd);
    aos_cli_register_command(&awss_cmd);
#ifdef EN_COMBO_NET
    aos_cli_register_command(&awss_ble_cmd);
    aos_cli_register_command(&ble_status_report_cmd);
#endif
    aos_cli_register_command(&linkkeycmd);
#endif

    aos_cli_register_commands(&mfg_cli_cmd[0], sizeof(mfg_cli_cmd) / sizeof(struct cli_command));


#ifdef EN_COMBO_NET
    combo_open();
#endif

    init_awss_flag();
    //init_msg_queue();
    Mars_dev_init();

    if (Mars_get_awss_flag() == 1) 
    {
        LOGW("mars", "发现待配网标志,设备开始进入配网状态)");        
        Mars_start_wifi_awss(1);
    }
    else
    {
        netmgr_ap_config_t ap_config;
        memset(&ap_config, 0, sizeof(netmgr_ap_config_t));
        netmgr_get_ap_config(&ap_config);
        if (strlen(ap_config.ssid) > 0) 
        {
            set_net_state(GOT_AP_SSID);
            // device_start_awss(0);
            LOGW("mars", "本地已保存wifi热点 = %s, 开始连接......\r\n", ap_config.ssid);
            aos_task_new("netmgr_start", start_netmgr, NULL, 5120);
            mars_dm_get_ctx()->status.NetState = NET_STATE_NOT_CONNECTED;
            mars_store_netstatus();
        }
        else
        {
            LOGW("mars", "本地未保存wifi热点!!!\r\n)");
            mars_dm_get_ctx()->status.NetState = NET_STATE_NOT_CONNECTED;
            mars_store_netstatus();
        }
    }

    aos_loop_run();

    return 0;
}
