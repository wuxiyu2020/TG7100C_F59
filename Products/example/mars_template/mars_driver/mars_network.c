/*
 * @Description  : 
 * @Author       : zhoubw
 * @Date         : 2022-03-15 17:11:40
 * @LastEditors  : zhoubw
 * @LastEditTime : 2022-11-17 16:56:24
 * @FilePath     : /tg7100c/Products/example/mars_template/mars_driver/mars_network.c
 */

#include <stdio.h>

#include <aos/aos.h>
#include "iot_export_linkkit.h"
#include "mars_network.h"
#include "../mars_devfunc/mars_devmgr.h"
// #include "cJSON.h"


#if defined(AOS_TIMER_SERVICE)||defined(AIOT_DEVICE_TIMER_ENABLE)
#include "iot_export_timer.h"
#endif
#ifdef EN_COMBO_NET
#include "../common/combo_net.h"
#include "breeze_export.h"
#endif
#include "../dev_config.h"
#include "../mars_driver/mars_cloud.h"

#define MARS_EV_NETWORK         (EV_USER + 0x0002)
#define RESPONE_BUFFER_SIZE     128

#define AWSS_TIMEOUT_MS              (3 * 60 * 1000)            //配网超时3分钟
#define CLEAR_AWSS_FLAG_TIMEOUT_MS   (30 * 1000)
#define CONNECT_AP_FAILED_TIMEOUT_MS (2 * 60 * 1000)

static mars_netchange_cb g_mars_netchange_callback = NULL;
static mars_property_set_cb g_mars_property_set_callback = NULL;
static mars_property_get_cb g_mars_property_get_callback = NULL;
extern int mars_ble_awss_state;
static int user_connected_event_handler(void)
{
    mprintf_d("user_connected_event_handler: 平台连接成功");
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();    
    mars_devmgr_afterConnect();
    wifi_mgmr_sta_powersaving(2);
    mars_template_ctx->cloud_connected = 1;
    aiot_ais_report_awss_status(AIS_AWSS_STATUS_PROGRESS_REPORT, AIS_AWSS_PROGRESS_CONNECT_MQTT_SUCCESS);
    set_net_state(CONNECT_CLOUD_SUCCESS);
    return 0;
}

static int user_disconnected_event_handler(void)
{
    mprintf_d("user_connected_event_handler: 平台断开连接!!!");
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();
    wifi_mgmr_sta_powersaving(0);
    set_net_state(CONNECT_CLOUD_FAILED);
    mars_template_ctx->cloud_connected = 0;
    return 0;
}

#define MAX_USER    5

struct Seq_Info {
    char userId[8];
    uint64_t timeStampe;
    int isValid;
};

static struct Seq_Info seqence_info[MAX_USER];

static int property_seq_handle(const char *seqenceId, recv_msg_t * msg)
{
    char userId[8], timeStr[16], *ptr;
    int isDiscard = 1, i;
    static int repalce_index = 0;

    if (strlen(seqenceId) > sizeof(msg->seq)) {
        mprintf_d("seq to long !!!");
        return -1;
    }
    if (NULL != (ptr = strchr(seqenceId, '@'))) {
        int len = ptr - seqenceId;

        if (len > (sizeof(userId) -1))
            len = sizeof(userId) -1;
        memset(userId, 0, sizeof(userId));
        memcpy(userId, seqenceId, len);

        len = strlen(seqenceId) - strlen(userId) - 1;
        if (len > (sizeof(timeStr) -1))
            len = sizeof(timeStr) -1;
        memset(timeStr, 0, sizeof(timeStr));
        memcpy(timeStr, ptr + 1, len);

        uint64_t time = atoll(timeStr);
        for (i = 0; i < MAX_USER; i++) {
            if (!strcmp(userId, seqence_info[i].userId)) {
                if (time >= seqence_info[i].timeStampe) {
                    isDiscard = 0;
                    seqence_info[i].timeStampe = time;
                }
                break;
            }
        }
        if (i == MAX_USER) {
            for (i = 0; i < MAX_USER; i++) {
                if (!seqence_info[i].isValid) {
                    strcpy(seqence_info[i].userId, userId);
                    seqence_info[i].timeStampe = time;
                    isDiscard = 0;
                    seqence_info[i].isValid = 1;
                    mprintf_d("new user %s", userId);
                    break;
                }
            }
            if (i == MAX_USER) {
                strcpy(seqence_info[repalce_index].userId, userId);
                seqence_info[repalce_index].timeStampe = time;
                isDiscard = 0;
                seqence_info[repalce_index].isValid = 1;
                repalce_index = (++repalce_index) % MAX_USER;
                mprintf_d("replace new user %s index %d", userId, repalce_index);
            }
        }
    }
    if (isDiscard == 1) {
        mprintf_d("Discard msg !!!");
        return -1;
    }
    if (NULL != seqenceId)
        strncpy(msg->seq, seqenceId, sizeof(msg->seq) - 1);

    return 0;
}

static int property_setting_handle(const char *request, const int request_len, recv_msg_t * msg)
{
    cJSON *root = NULL, *item = NULL;
    int ret = -1;

    if ((root = cJSON_Parse(request)) == NULL) {
        mprintf_d("property set payload is not JSON format");
        return -1;
    }

    char* tmp_str = cJSON_Print(root);
    if (tmp_str != NULL)
    {
        mprintf_d("平台下发: %s", tmp_str);
        cJSON_free(tmp_str);
    }

    if ((item = cJSON_GetObjectItem(root, "setPropsExtends")) != NULL && cJSON_IsObject(item)) {
        int isDiscard = 0;
        cJSON *seq = NULL, *flag = NULL;

        if ((seq = cJSON_GetObjectItem(item, "seq")) != NULL && cJSON_IsString(seq)) {
            if (property_seq_handle(seq->valuestring, msg)) {
                isDiscard = 1;
            }
        }
        if (isDiscard == 0) {
            if ((flag = cJSON_GetObjectItem(item, "flag")) != NULL && cJSON_IsNumber(flag)) {
                msg->flag = flag->valueint;
            } else {
                msg->flag = 0;
            }
        } else {
            cJSON_Delete(root);
            return 0;
        }
    }

#if 1
    ret = g_mars_property_set_callback(root, item, msg);
#else    
    if ((item = cJSON_GetObjectItem(root, "powerstate")) != NULL && cJSON_IsNumber(item)) {
        msg->powerswitch = item->valueint;
        msg->all_powerstate = msg->powerswitch;
        ret = 0;
    }
#ifdef TSL_FY_SUPPORT /* support old feiyan TSL */
    else if ((item = cJSON_GetObjectItem(root, "PowerSwitch")) != NULL && cJSON_IsNumber(item)) {
        msg->powerswitch = item->valueint;
        ret = 0;
    }
#endif
    else if ((item = cJSON_GetObjectItem(root, "allPowerstate")) != NULL && cJSON_IsNumber(item)) {
        msg->powerswitch = item->valueint;
        msg->all_powerstate = msg->powerswitch;
        ret = 0;
    }
#ifdef AOS_TIMER_SERVICE
    else if (((item = cJSON_GetObjectItem(root, "LocalTimer")) != NULL && cJSON_IsArray(item)) || \
        ((item = cJSON_GetObjectItem(root, "CountDownList")) != NULL && cJSON_IsObject(item)) || \
        ((item = cJSON_GetObjectItem(root, "PeriodTimer")) != NULL && cJSON_IsObject(item)) || \
        ((item = cJSON_GetObjectItem(root, "RandomTimer")) != NULL && cJSON_IsObject(item)))
    {
        // Before LocalTimer Handle, Free Memory
        cJSON_Delete(root);
        timer_service_property_set(request);
        mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();
        IOT_Linkkit_Report(mars_template_ctx->master_devid, ITM_MSG_POST_PROPERTY,
                (unsigned char *)request, request_len);
        return 0;
    }
#endif
#ifdef AIOT_DEVICE_TIMER_ENABLE
    else if ((item = cJSON_GetObjectItem(root, DEVICETIMER)) != NULL && cJSON_IsArray(item))
    {
        // Before LocalTimer Handle, Free Memory
        cJSON_Delete(root);
        ret = deviceTimerParse(request, 0, 1);
        mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();
        if (ret == 0) {
            IOT_Linkkit_Report(mars_template_ctx->master_devid, ITM_MSG_POST_PROPERTY,
                (unsigned char *)request, request_len);
        } else {
            char *report_fail = "{\"DeviceTimer\":[]}";
            IOT_Linkkit_Report(mars_template_ctx->master_devid, ITM_MSG_POST_PROPERTY,
                    (unsigned char *)report_fail, strlen(report_fail));
            ret = -1;
        }
        // char *property = device_timer_post(1);
        // if (property != NULL)
        //     HAL_Free(property);
        return 0;
    }
    #ifdef MULTI_ELEMENT_TEST
    else if (propertys_handle(root) >= 0) {
        mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();
        cJSON_Delete(root);
        IOT_Linkkit_Report(mars_template_ctx->master_devid, ITM_MSG_POST_PROPERTY,
                (unsigned char *)request, request_len);
        return 0;
    }
    #endif
#endif
    else {
        mprintf_d("property set payload is not JSON format");
        ret = -1;
    }
#endif
    cJSON_Delete(root);

    // if (ret != -1)
    //     send_msg_to_queue(msg);

    return ret;
}

static int user_service_request_event_handler(const int devid, const char *serviceid, const int serviceid_len,
        const char *request, const int request_len,
        char **response, int *response_len)
{
    cJSON *root = NULL;

#ifdef CERTIFICATION_TEST_MODE
    return ct_main_service_request_event_handler(devid, serviceid, serviceid_len, request, request_len, response, response_len);
#endif

    mprintf_d("Service Request Received, Devid: %d, Service ID: %.*s, Payload: %s", devid, serviceid_len, serviceid,
            request);

    root = cJSON_Parse(request);
    if (root == NULL || !cJSON_IsObject(root)) {
        mprintf_d("JSON Parse Error");
        return -1;
    }
    if (strlen("CommonService") == serviceid_len && memcmp("CommonService", serviceid, serviceid_len) == 0) {
        cJSON *item;
        const char *response_fmt = "{}";
        recv_msg_t msg;
        int isDiscard = 0;

        strcpy(msg.seq, SPEC_SEQ);
        if ((item = cJSON_GetObjectItem(root, "seq")) != NULL && cJSON_IsString(item)) {
            if (property_seq_handle(item->valuestring, &msg)) {
                isDiscard = 1;
            }
        }
        if (isDiscard == 0) {
            if ((item = cJSON_GetObjectItem(root, "flag")) != NULL && cJSON_IsNumber(item)) {
                msg.flag = item->valueint;
            } else {
                msg.flag = 0;
            }

            if ((item = cJSON_GetObjectItem(root, "method")) != NULL && cJSON_IsNumber(item)) {
                msg.method = item->valueint;
            } else {
                msg.method = 0;
            }

            if ((item = cJSON_GetObjectItem(root, "params")) != NULL && cJSON_IsString(item)) {
                if (msg.method == 0) {
                    // msg.from = FROM_SERVICE_SET;
                    property_setting_handle(item->valuestring, strlen(item->valuestring), &msg);
                } else
                    mprintf_d("todo!!");
            }
        }
        *response = HAL_Malloc(RESPONE_BUFFER_SIZE);
        if (*response == NULL) {
            mprintf_d("Memory Not Enough");
            cJSON_Delete(root);
            return -1;
        }
        memset(*response, 0, RESPONE_BUFFER_SIZE);
        memcpy(*response, response_fmt, strlen(response_fmt));
        *response_len = strlen(*response);
    }
    cJSON_Delete(root);
    return 0;
}

static int user_property_set_event_handler(const int devid, const char *request, const int request_len)
{
    int ret = 0;
    recv_msg_t msg = {0};

#ifdef CERTIFICATION_TEST_MODE
    return ct_main_property_set_event_handler(devid, request, request_len);
#endif
    
    msg.from = FROM_PROPERTY_SET;
    strcpy(msg.seq, SPEC_SEQ);
    property_setting_handle(request, request_len, &msg);
    return ret;
}

#ifdef ALCS_ENABLED
static int user_property_get_event_handler(const int devid, const char *request, const int request_len, char **response,
        int *response_len)
{
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();
    dev_status_t *device_status = &mars_template_ctx->status;
    cJSON *request_root = NULL, *item_propertyid = NULL;
    cJSON *response_root = NULL;

#ifdef CERTIFICATION_TEST_MODE
    return ct_main_property_get_event_handler(devid, request, request_len, response, response_len);
#endif

    mprintf_d("Property Get Received, Devid: %d, Request: %s", devid, request);
    request_root = cJSON_Parse(request);
    if (request_root == NULL || !cJSON_IsArray(request_root)) {
        mprintf_d("JSON Parse Error");
        return -1;
    }

    response_root = cJSON_CreateObject();
    if (response_root == NULL) {
        mprintf_d("No Enough Memory");
        cJSON_Delete(request_root);
        return -1;
    }
    int index = 0;
    for (index = 0; index < cJSON_GetArraySize(request_root); index++) {
        item_propertyid = cJSON_GetArrayItem(request_root, index);
        if (item_propertyid == NULL || !cJSON_IsString(item_propertyid)) {
            mprintf_d("JSON Parse Error");
            cJSON_Delete(request_root);
            cJSON_Delete(response_root);
            return -1;
        }
        mprintf_d("Property ID, index: %d, Value: %s", index, item_propertyid->valuestring);
    }

    cJSON_Delete(request_root);

    *response = cJSON_PrintUnformatted(response_root);
    if (*response == NULL) {
        mprintf_d("cJSON_PrintUnformatted Error");
        cJSON_Delete(response_root);
        return -1;
    }
    cJSON_Delete(response_root);
    *response_len = strlen(*response);

    mprintf_d("Property Get Response: %s", *response);
    return SUCCESS_RETURN;
}
#endif

static int user_report_reply_event_handler(const int devid, const int msgid, const int code, const char *reply,
        const int reply_len)
{
    //const char *reply_value = (reply == NULL) ? ("NULL") : (reply);
    //const int reply_value_len = (reply_len == 0) ? (strlen("NULL")) : (reply_len);

    //mprintf_d("Message Post Reply Received, Devid: %d, Message ID: %d, Code: %d, Reply: %.*s", devid, msgid, code,
    //        reply_value_len, reply_value);
    return 0;
}

static int user_trigger_event_reply_event_handler(const int devid, const int msgid, const int code, const char *eventid,
        const int eventid_len, const char *message, const int message_len)
{
    mprintf_d("Trigger Event Reply Received, Devid: %d, Message ID: %d, Code: %d, EventID: %.*s, Message: %.*s", devid,
            msgid, code, eventid_len, eventid, message_len, message);

    return 0;
}

static int user_initialized(const int devid)
{
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();
    mprintf_d("Device Initialized, Devid: %d", devid);

    if (mars_template_ctx->master_devid == devid) {
        mars_template_ctx->master_initialized = 1;
    }

    return 0;
}

static int notify_msg_handle(const char *request, const int request_len)
{
    cJSON *request_root = NULL;
    cJSON *item = NULL;

    request_root = cJSON_Parse(request);
    if (request_root == NULL) 
    {
        mprintf_d("JSON Parse Error");
        return -1;
    }

    item = cJSON_GetObjectItem(request_root, "identifier");
    if (item == NULL || !cJSON_IsString(item)) 
    {
        cJSON_Delete(request_root);
        return -1;
    }

    if (!strcmp(item->valuestring, "awss.BindNotify")) 
    {
        cJSON *value = cJSON_GetObjectItem(request_root, "value");
        if (value == NULL || !cJSON_IsObject(value)) 
        {
            cJSON_Delete(request_root);
            return -1;
        }
        
        cJSON *op = cJSON_GetObjectItem(value, "Operation");
        if (op != NULL && cJSON_IsString(op)) 
        {
            if (!strcmp(op->valuestring, "Bind")) 
            {
                LOGW(PRO_NAME, "平台通知: 本设备被绑定");
                // vendor_device_bind();
                set_net_state(APP_BIND_SUCCESS);
            } 
            else if (!strcmp(op->valuestring, "Unbind")) 
            {
                LOGE(PRO_NAME, "平台通知: 本设备被解绑");
                // vendor_device_unbind();
                linkkit_reset(NULL);
            } 
            else if (!strcmp(op->valuestring, "Reset")) 
            {
                LOGW(PRO_NAME, "Device reset");
                // vendor_device_reset();
                //set_net_state(APP_BIND_SUCCESS);
            }
        }
    }

    cJSON_Delete(request_root);
    return 0;
}

static int user_event_notify_handler(const int devid, const char *request, const int request_len)
{
    int res = 0;
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();
    LOGW("mars", "Event notify Received, Devid: %d, Request: %s", devid, request);

    notify_msg_handle(request, request_len);
    res = IOT_Linkkit_Report(mars_template_ctx->master_devid, ITM_MSG_EVENT_NOTIFY_REPLY,
            (unsigned char *)request, request_len);
    LOGW("mars", "发送事件应答. Message ID: %d", res);

    return 0;
}

static aos_timer_t awss_timeout_timer;
static uint8_t awss_run_state = 0;                  /* 1-awss running, 0-awss is idle */

static int dev_pre_net_state = UNKNOW_STATE;
static int dev_cur_net_state = UNKNOW_STATE;

bool is_awss_state(void)
{
    return (awss_timeout_timer.hdl != NULL);
}

int get_net_state(void)
{
    return dev_cur_net_state;
}

void set_net_state(int state)
{
    dev_pre_net_state = dev_cur_net_state;
    dev_cur_net_state = state;

    if (dev_pre_net_state != dev_cur_net_state)
    {        
        aos_post_event(MARS_EV_NETWORK, state, 0);
    }
}

/* discoverable_awss_mode: enable discoverable provision mode flag, dev-ap or ble-awss 
    0 - all awss mode
    1 - only discoverable provision mode */
extern int mars_ble_awss_state;
void device_start_awss(uint8_t discoverable_awss_mode)
{
    if (awss_run_state == 0) {
#if defined (AWSS_ONESHOT_MODE)
        awss_config_press();
        do_awss();
#elif defined (AWSS_DEV_AP_MODE)
        do_awss_dev_ap();
#elif defined (AWSS_BT_MODE)
        // for combo device, ble_awss and smart_config awss mode can exist simultaneously
            mars_dm_get_ctx()->status.NetState = NET_STATE_CONNECTING;
            mars_store_netstatus();
        if (discoverable_awss_mode) {
            /* should re-open discoverable provision mode when has ap-info */
            mprintf_d("device_start_awss: 进入蓝牙配网状态\r\n");
            combo_set_ap_state(COMBO_AP_DISCONNECTED);
            do_ble_awss();
        } else {
            mprintf_d("device_start_awss: 进入多通道配网状态\r\n");
            do_ble_awss();
            awss_config_press();
            do_awss();
        }
#else
#warning "Unsupported awss mode!!!"
#endif
    }

    awss_run_state = 1;
}

void device_stop_awss(void)
{
    if (awss_run_state != 0) {
#if defined (AWSS_ONESHOT_MODE)
        awss_stop();
#elif defined (AWSS_DEV_AP_MODE)
        awss_dev_ap_stop(); 
#elif defined (AWSS_BT_MODE)
        // for combo device, ble_awss and smart_config awss mode can exist simultaneously
        awss_stop();
        combo_set_awss_state(0);
#ifdef AWSS_SUPPORT_DEV_AP
        awss_dev_ap_stop(); 
#endif
#else
#warning "Unsupported awss mode!!!"
#endif
    }

    awss_run_state = 0;
}

static unsigned char awss_flag  = 0;
void timer_func_awss_finish(void *arg1, void *arg2)
{
    if (mars_dm_get_ctx()->status.TokenRptFlag == 0)
    {
        LOGE("mars", "配网失败. failed!!!");
        mars_dm_get_ctx()->status.NetState = NET_STATE_NOT_CONNECTED;
        mars_store_netstatus();       
    }
    else
    {
        if (is_awss_state()) 
        {
            extern int xm_open_voice_file(char *file);
            LOGI("mars", "播报: 设备联网成功");
            xm_open_voice_file("114");
        }
    }

    if (awss_timeout_timer.hdl != NULL)
    {
        aos_timer_stop(&awss_timeout_timer);
        aos_timer_free(&awss_timeout_timer);
        LOGE("mars", "关闭配网定时器 (time=%d秒)", AWSS_TIMEOUT_MS/1000);
    }

    
    set_net_state(AWSS_NOT_START);
    device_stop_awss();
}

int init_awss_flag(void)
{
    unsigned char value;
    int ret, len = sizeof(value);

    ret = aos_kv_get("awss_flag", &value, &len);
    if (ret == 0 && len > 0) {
        awss_flag = value;
    }
    return 0;
}

unsigned char Mars_get_awss_flag(void)
{
    return awss_flag;
}

int get_bind_flag(void)
{
    unsigned char value = 0;
    int len = sizeof(value);

    int ret = aos_kv_get("bind_flag", &value, &len);
    if (ret == 0 && len > 0) {
        return value;
    }
    return 0;
}

int set_bind_flag(void)
{
    unsigned char bind_flag = 1;
    return aos_kv_set("bind_flag", &bind_flag, 1, 1);
}

int del_bind_flag(void)
{
    aos_kv_del("bind_flag");
}

static void mars_netchange_event(input_event_t *eventinfo, void *priv_data)
{
    if (eventinfo->type != MARS_EV_NETWORK) {
        return;
    }

    if (g_mars_netchange_callback){
        g_mars_netchange_callback(eventinfo->code);
    }
    
    switch (eventinfo->code)
    {
        case RECONFIGED:
            LOGW("mars", "网络状态: RECONFIGED %d", RECONFIGED);
            break;

        case UNCONFIGED:
            LOGW("mars", "网络状态: UNCONFIGED %d", UNCONFIGED);
            break;

        case AWSS_NOT_START:
            LOGW("mars", "网络状态: AWSS_NOT_START %d", AWSS_NOT_START);
            break;

        case GOT_AP_SSID:
            LOGW("mars", "网络状态: GOT_AP_SSID %d", GOT_AP_SSID);
            break;

        case CONNECT_CLOUD_FAILED:
            LOGW("mars", "网络状态: CONNECT_CLOUD_FAILED %d", CONNECT_CLOUD_FAILED);
            break;

        case CONNECT_AP_FAILED_TIMEOUT:
            LOGW("mars", "网络状态: CONNECT_AP_FAILED_TIMEOUT %d", CONNECT_AP_FAILED_TIMEOUT);
            break;

        case CONNECT_AP_FAILED:
            LOGW("mars", "网络状态: CONNECT_AP_FAILED %d", CONNECT_AP_FAILED);
            break;

        case CONNECT_CLOUD_SUCCESS:
            LOGW("mars", "网络状态: CONNECT_CLOUD_SUCCESS %d", CONNECT_CLOUD_SUCCESS);
            break;

        case APP_BIND_SUCCESS:
            LOGW("mars", "网络状态: APP_BIND_SUCCESS %d", APP_BIND_SUCCESS);
            //set_bind_flag();
            //timer_func_awss_finish(NULL, NULL);
            //set_net_state(CONNECT_CLOUD_SUCCESS);
            break;

        case FACTORY_BEGIN:
            LOGW("mars", "网络状态: FACTORY_BEGIN %d", FACTORY_BEGIN);
            set_net_state(FACTORY_SUCCESS);
            break;

        case FACTORY_SUCCESS:
            LOGW("mars", "网络状态: FACTORY_SUCCESS %d", FACTORY_SUCCESS);
            break;

        case FACTORY_FAILED_1:
            LOGW("mars", "网络状态: FACTORY_FAILED_1 %d", FACTORY_FAILED_1);
            break;

        case FACTORY_FAILED_2:
            LOGW("mars", "网络状态: FACTORY_FAILED_2 %d", FACTORY_FAILED_2);
            break;

        default:
            break;
    }
}

void Mars_start_wifi_awss(uint8_t discoverable_awss_mode)
{
    mars_dm_get_ctx()->status.TokenRptFlag = 0;
    aos_kv_del("awss_flag");
    aos_timer_new_ext(&awss_timeout_timer, timer_func_awss_finish, NULL, AWSS_TIMEOUT_MS, 0, 1);
    device_start_awss(discoverable_awss_mode);

    aos_msleep(2000);
    extern int xm_open_voice_file(char *file);
    LOGI("mars", "播报: 设备已进入联网状态");
    xm_open_voice_file("112");
}

/**
 * @brief 
 * @param {mars_netchange_cb} *netchange_cb
 * @param {mars_property_set_cb} *property_set_cb
 * @param {mars_property_get_cb} *property_get_cb
 * @return {*}
 */
void Mars_network_init( mars_property_set_cb property_set_cb,  mars_property_get_cb property_get_cb)
{
    IOT_RegisterCallback(ITE_CONNECT_SUCC, user_connected_event_handler);
    IOT_RegisterCallback(ITE_DISCONNECTED, user_disconnected_event_handler);
#ifdef CERTIFICATION_TEST_MODE
    IOT_RegisterCallback(ITE_RAWDATA_ARRIVED, ct_main_down_raw_data_arrived_event_handler);
#endif
    IOT_RegisterCallback(ITE_SERVICE_REQUEST, user_service_request_event_handler);
    IOT_RegisterCallback(ITE_PROPERTY_SET, user_property_set_event_handler);
#ifdef ALCS_ENABLED
    /*Only for local communication service(ALCS) */
    IOT_RegisterCallback(ITE_PROPERTY_GET, user_property_get_event_handler);
#endif
    IOT_RegisterCallback(ITE_REPORT_REPLY, user_report_reply_event_handler);
    IOT_RegisterCallback(ITE_TRIGGER_EVENT_REPLY, user_trigger_event_reply_event_handler);
    // IOT_RegisterCallback(ITE_TIMESTAMP_REPLY, user_timestamp_reply_event_handler);
    IOT_RegisterCallback(ITE_INITIALIZE_COMPLETED, user_initialized);
    // IOT_RegisterCallback(ITE_FOTA, user_fota_event_handler);
    IOT_RegisterCallback(ITE_EVENT_NOTIFY, user_event_notify_handler);
#if defined (CLOUD_OFFLINE_RESET)
    IOT_RegisterCallback(ITE_OFFLINE_RESET, user_offline_reset_handler);
#endif

    if (property_set_cb){
        g_mars_property_set_callback = property_set_cb;
    }

    if (property_get_cb){
        g_mars_property_get_callback = property_get_cb;
    }

    aos_register_event_filter(MARS_EV_NETWORK, mars_netchange_event, NULL);
}


