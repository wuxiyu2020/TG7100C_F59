/*
 * @Author: zhoubw
 * @Date: 2022-03-11 11:44:05
 * @LastEditors  : zhoubw
 * @LastEditTime : 2022-11-24 17:04:48
 * @FilePath     : /tg7100c/Products/example/mars_template/mars_driver/mars_cloud.c
 * @Description: 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <time.h>

#include <aos/aos.h>
#include <aos/yloop.h>
#include "netmgr.h"
#include "iot_export.h"
#include "iot_import.h"

#include "iot_export_linkkit.h"
#include "linkkit_export.h"
#include "cJSON.h"
#include "aos/kv.h"
#include "iot_export.h"
#include "../dev_config.h"
#include "vendor.h"
// #include "device_state_manger.h"
#include "../mars_devfunc/mars_devmgr.h"
#include "../mars_driver/mars_uartmsg.h"
#include "mars_cloud.h"
#ifdef EN_COMBO_NET
#include "../common/combo_net.h"
#endif

extern void mbedtls_md5(const unsigned char *input, size_t ilen, unsigned char output[16]);


#define DEFAULT_TIMEZONE   8
#define TIMER_UPDATE_INTERVAL   (12 * 3600 * 1000)

static aos_timer_t ntp_timer = {.hdl = NULL};
static uint64_t ntp_time_ms = 0;
static uint64_t basic_time = 0;
static char g_cloud_token[60] = {0};
static char g_log_sign[60] = {0};

static char time_buff[32] = {0x00};
char* get_current_time()
{
    uint64_t time = (ntp_time_ms + 28800000 + aos_now_ms() - basic_time);
    time_t t = (time / 1000);  //122122 115038
    struct tm *cur_tm = localtime(&t);
    memset(time_buff, 0x00, sizeof(time_buff));
    snprintf(time_buff, sizeof(time_buff), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
    cur_tm->tm_year+1900,
    cur_tm->tm_mon+1, 
    cur_tm->tm_mday, 
    cur_tm->tm_hour, 
    cur_tm->tm_min, 
    cur_tm->tm_sec,
    (int)(time%1000)); //坑：这里如果写time%1000，不强转的话，就会输出 2023-03-09 16:23:45.588205944
    return time_buff;
}

void mars_store_time_date(void)
{
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();
    time_t t = ((ntp_time_ms + 28800000 + aos_now_ms() - basic_time) / 1000);  //122122 115038
    struct tm *cur_tm = localtime(&t);

    int buf_len = 0;
    uint8_t buf_setmsg[10] = {0};
    buf_setmsg[buf_len++] = prop_ServeTime;
    buf_setmsg[buf_len++] = cur_tm->tm_hour;
    buf_setmsg[buf_len++] = cur_tm->tm_min;
    buf_setmsg[buf_len++] = cur_tm->tm_sec;
    LOGI("mars", "发送时间到腰部: %02d:%02d:%02d", cur_tm->tm_hour, cur_tm->tm_min, cur_tm->tm_sec);
    Mars_uartmsg_send(cmd_store, uart_get_seq_mid(),  buf_setmsg, buf_len, 3);

    buf_len = 0;
    buf_setmsg[buf_len++] = prop_ServeDate;
    buf_setmsg[buf_len++] = ((cur_tm->tm_year+1900) % 100);
    buf_setmsg[buf_len++] = cur_tm->tm_mon+1;
    buf_setmsg[buf_len++] = cur_tm->tm_mday;
    LOGI("mars", "发送日期到腰部: %04d-%02d-%02d", cur_tm->tm_year+1900, cur_tm->tm_mon+1, cur_tm->tm_mday);
    Mars_uartmsg_send(cmd_store, uart_get_seq_mid(),  buf_setmsg, buf_len, 3);
}

void ntp_timer_update(const char *str)  //相对于1900年1月1日00:00:00的偏移时间, 以ms为单位
{
    if (strlen(str) < 13)
        return;
    ntp_time_ms = (uint64_t)strtoll(str, NULL, 10);
    basic_time = aos_now_ms();

    time_t t = ((ntp_time_ms + 28800000) / 1000);
    struct tm *cur_tm = localtime(&t);
    LOGI("mars", "已拉取网络时间: 时间戳 = %lu", ((ntp_time_ms + 28800000) / 1000));
    LOGI("mars", "已拉取网络时间: 时间 = %04d-%02d-%02d %02d:%02d:%02d\r\n", 
    cur_tm->tm_year+1900,
    cur_tm->tm_mon+1, 
    cur_tm->tm_mday, 
    cur_tm->tm_hour, 
    cur_tm->tm_min, 
    cur_tm->tm_sec);

    int buf_len = 0;
    uint8_t buf_setmsg[10] = {0};
    buf_setmsg[buf_len++] = prop_ServeTime;
    buf_setmsg[buf_len++] = cur_tm->tm_hour;
    buf_setmsg[buf_len++] = cur_tm->tm_min;
    buf_setmsg[buf_len++] = cur_tm->tm_sec;
    LOGI("mars", "发送时间到腰部: %02d:%02d:%02d", cur_tm->tm_hour, cur_tm->tm_min, cur_tm->tm_sec);
    Mars_uartmsg_send(cmd_store, uart_get_seq_mid(),  buf_setmsg, buf_len, 3);

    buf_len = 0;
    buf_setmsg[buf_len++] = prop_ServeDate;
    buf_setmsg[buf_len++] = ((cur_tm->tm_year+1900) % 100);
    buf_setmsg[buf_len++] = cur_tm->tm_mon+1;
    buf_setmsg[buf_len++] = cur_tm->tm_mday;
    LOGI("mars", "发送日期到腰部: %04d-%02d-%02d", cur_tm->tm_year+1900, cur_tm->tm_mon+1, cur_tm->tm_mday);
    Mars_uartmsg_send(cmd_store, uart_get_seq_mid(),  buf_setmsg, buf_len, 3);
}

int64_t mars_getTimestamp_ms(void)
{
    int64_t timestamp = 0;
    if (ntp_time_ms){
        timestamp = ntp_time_ms + aos_now_ms() - basic_time;
    }else{
        timestamp = -1;
    }
    return timestamp;
}

void get_cloud_time(void *arg1, void *arg2)
{
    LOGI("mars", "开始拉取网络时间");
    linkkit_ntp_time_request(ntp_timer_update);
}

void get_cloud_time_timer_start(void)
{
    if (ntp_timer.hdl == NULL)
    {
        LOGI("mars", "拉取网络时间 定时器启动......(周期=%d小时)", TIMER_UPDATE_INTERVAL/(3600 * 1000));
        aos_timer_new_ext(&ntp_timer, get_cloud_time, NULL, TIMER_UPDATE_INTERVAL, 1, 1);
    }
}

/**
 * @brief 上报事件到云端
 * @param devid 由调用IOT_Linkkit_Open返回的设备标示符
 */
int32_t user_post_event_json(device_event_t id)
{
    #define EVENT_CODE_PARAMS "{\"Code\":%d}"
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx(); 
    char payload[64] = {0};
    sprintf(payload, EVENT_CODE_PARAMS, id);    
    char* eventid = "JczEventPush";
    return IOT_Linkkit_TriggerEvent(mars_template_ctx->master_devid, eventid, strlen(eventid), payload, strlen(payload));
}

/**
 * @brief 上报属性到云端
 * @param devid 由调用IOT_Linkkit_Open返回的设备标示符
 */
int32_t user_post_property_json(const char *property)
{
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();
    return IOT_Linkkit_Report(mars_template_ctx->master_devid, ITM_MSG_POST_PROPERTY, (unsigned char *)property, strlen(property));
}

//log post
//#define STR_LOGPOST  "{\"deviceName\":\"%s\",\"mac\":\"%s\",\"startTime\":\"%ld\",\"stopTime\":\"%ld\",\"logContent\":\"%s\",\"softVersion\":\"%s\",\"hardVersion\":\"1.3.0\"}"
#define STR_LOGPOST  "{\"deviceName\":\"%s\",\"mac\":\"%s\",\"startTime\":\"%ld\",\"stopTime\":\"%ld\",\"logContent\":\"%s\",\"ifException\":\"%d\",\"softVersion\":\"%s\"}"


// #define URL_LOGPOST  "192.168.1.108:12300"
// #define URL_LOGPOST  "http://mcook.iotmars.com/api/log/upload/device"
#define LOG_BUFMAX  1024
// char g_logbuf[LOG_BUFMAX] = "Power on and first connect!!!!\r\n";
// int g_logbuf_len = strlen("Power on and first connect!!!!\r\n");
char g_logbuf[LOG_BUFMAX] = {};
int g_logbuf_len = 0;
int64_t log_startTime = 0;
void mars_cloud_logpost(char *log_msg, int log_len)
{
    if (NULL == log_msg)
    {
        return;
    }
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();
	int ifException = 0;
    //if (0 == mars_template_ctx->cloud_connected || !mars_getTimestamp_ms()){
    if (!netmgr_get_ip_state || !mars_getTimestamp_ms())
    {
        return;
    }

    if ((g_logbuf_len + log_len) < LOG_BUFMAX)
    {
        //长度没有溢出，继续添加消息
        if (0 == g_logbuf_len)
        {
            log_startTime = mars_getTimestamp_ms();
        }
        strncpy(g_logbuf+g_logbuf_len, log_msg, log_len);
        g_logbuf_len += log_len;
    }
    else
    {
        http_msg_t log_http = {0};
        char *http_log_msg = (char *)aos_malloc(g_logbuf_len + 250);
        if (http_log_msg)
        {
            int64_t log_nowtime = mars_getTimestamp_ms();
            if (0 == log_startTime)
            {
                log_startTime = log_nowtime;
            }

            // 老版本上报内容
            // sprintf(http_log_msg,
            //         STR_LOGPOST,
            //         mars_template_ctx->device_name,
            //         mars_template_ctx->macStr,
            //         (uint32_t)(log_startTime/1000),
            //         (uint32_t)(log_nowtime/1000),
            //         g_logbuf,
            //         aos_get_app_version());

            /* 获取上报日志 */
            sprintf(http_log_msg,
                    STR_LOGPOST,
                    mars_template_ctx->device_name,
                    mars_template_ctx->macStr,
                    (uint32_t)(log_startTime/1000),
                    (uint32_t)(log_nowtime/1000),
                    g_logbuf,
                    ifException,
                    aos_get_app_version());

            LOGI("mars", "日志上报发送: %s", http_log_msg);

            log_http.http_method = HTTP_LOGPOST;
            log_http.msg_str = http_log_msg;
            log_http.msg_len = g_logbuf_len+250;
            send_http_to_queue(&log_http);
        }

        memset(g_logbuf, 0, LOG_BUFMAX);
        log_startTime = mars_getTimestamp_ms();
        strcpy(g_logbuf, log_msg);
        g_logbuf_len = log_len;
    }
}


//http
int mars_http_getpost(uint8_t method, const char *http_url, httpc_req_t *http_req, char *msg_out, uint16_t out_len, uint32_t timeout)
{
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();
    // if (1 != mars_template_ctx->bind_notified){
    //     return 0;
    // }

    int ret = -1;
    http_session_t http_handle = 0;
    parsed_url_t t_url = {0};
    int read_size = 0;

    char *tmpbuf = (char *)aos_malloc(256);
    //LOGI("mars", "mars_http_getpost: aos_malloc = 0x%08X", tmpbuf);
    if (tmpbuf){
        bzero(tmpbuf, 256);
        ret = http_parse_URL(http_url, tmpbuf, 256, &t_url);
        if (0 == ret){
            ret = http_open_session(&http_handle, http_url, 0, NULL, 3);
            if (0 == ret){
                if (http_req){
                    http_req->resource = t_url.resource;
                    if (HTTP_TEMPCURVE == method || HTTP_COOKHISTORY == method){
                        ret = http_prepare_req(http_handle, http_req, HDR_ADD_CONN_KEEP_ALIVE);
                    }else{
                        ret = http_prepare_req(http_handle, http_req, HDR_ADD_CONN_CLOSE);
                    }
                    if (0 == ret){
                        http_add_header(http_handle, "Content-Type", "application/json");
                        if (HTTP_LOGPOST == method){        //日志上报需要添加head,暂时在通用接口中做判断,后续再优化
                            http_add_header(http_handle, "category", DEV_CATEGORY);

                            // char dev_model[PRODUCT_KEY_LEN+1] = {0};
                            // if (!strncmp(PK_2, mars_template_ctx->product_key, (PRODUCT_KEY_LEN+1))){
                            //     strncpy(dev_model, "E70BCZ01", sizeof(dev_model));  //E70两体
                            // }else{
                            //     strncpy(dev_model, "E70BC01", sizeof(dev_model));   //默认E70一体
                            // }
                            char dev_model[PRODUCT_KEY_LEN+1] = {0};
                            strncpy(dev_model, get_product_name(), sizeof(dev_model));   
                            http_add_header(http_handle, "model", dev_model);
                            // http_add_header(http_handle, "model", DEV_MODEL);

                            http_add_header(http_handle, "sign", mars_cloud_logsignget());        //根据协议,sign直接用固定字符串
                        }

                        ret = http_send_request(http_handle, http_req);
                        if (0 == ret){
                            if (msg_out){
                                bzero(msg_out, out_len);
                                read_size = http_read_content_timeout(http_handle, msg_out, out_len, timeout);
                                if (read_size != 0){
                                    LOGI("mars", "日志上报接收 :%s", msg_out);
                                }
                            }else{
                                mprintf_e("msg_out not exist");
                            }
                        }else{
                            mprintf_e("http_send_request ret=%d", ret);
                        }
                    }else{
                        mprintf_e("http_prepare_req ret=%d", ret);
                    }
                }else{
                    mprintf_e("http_req not exist");
                }

            }else{
                mprintf_e("http_open_session ret=%d", ret);
            }

            if (http_handle){
                http_close_session(&http_handle);
            }
            http_handle = 0;
        }else{
            mprintf_e("http_parse_URL ret=%d", ret);
        }

        aos_free(tmpbuf);
        //LOGI("mars", "mars_http_getpost: aos_free = 0x%08X", tmpbuf);
    }else{
        mprintf_e("malloc failed.");
    }

    tmpbuf = NULL;  
    return read_size;
}

//http 
aos_queue_t *g_mars_http_queue_id = NULL;
static char *g_mars_http_queue_buff = NULL;

void init_http_queue(void)
{
    #define MB_RGBSTATUS_COUNT 6
    if (g_mars_http_queue_buff == NULL) 
    {
        g_mars_http_queue_id = (aos_queue_t *) aos_malloc(sizeof(aos_queue_t));
        g_mars_http_queue_buff = aos_malloc(MB_RGBSTATUS_COUNT * sizeof(http_msg_t));
        aos_queue_new(g_mars_http_queue_id, g_mars_http_queue_buff, MB_RGBSTATUS_COUNT * sizeof(http_msg_t), sizeof(http_msg_t));
    }
}

void send_http_to_queue(http_msg_t *http_msg)
{
    int ret = aos_queue_send(g_mars_http_queue_id, http_msg, sizeof(http_msg_t));
    if (ret != 0)
    {
        aos_free(http_msg->msg_str);
        LOGE("mars", "上报消息加入队列失败!!! error\r\n");
    }
}

void http_process_task(void *argv)
{
    unsigned int rcvLen;
    http_msg_t http_msg;
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();

    while (true) {
        if (aos_queue_recv(g_mars_http_queue_id, AOS_WAIT_FOREVER, &http_msg, &rcvLen) == 0) {
            char http_url[256] = {0};
            if (HTTP_LOGPOST == http_msg.http_method){
                strncpy(http_url, URL_LOGPOST, sizeof(http_url));
            }else if (HTTP_TEMPCURVE == http_msg.http_method){
                strncpy(http_url, URL_TEMPERPOST, sizeof(http_url));
            }else if (HTTP_COOKHISTORY == http_msg.http_method){
                strncpy(http_url, URL_COOKHISTORYPOST, sizeof(http_url));
            }
            else
            {
                mprintf_e("未处理分支,会导致内存泄露\r\n");
            }

            if (strlen(http_url)){
                httpc_req_t http_req = {
                .type = HTTP_POST,
                .version = HTTP_VER_1_1,
                .resource = NULL,
                .content = NULL,
                .content_len = 0,
                };

                // if (HTTP_COOKHISTORY == http_msg.http_method){
                //     http_req.type = HTTP_GET;
                // }
                http_req.content = http_msg.msg_str;
                if (http_req.content){

                    http_req.content_len = strlen(http_msg.msg_str);
                    char *recv_msg = (char *)aos_malloc(512);
                    if (recv_msg){
                        mars_http_getpost(http_msg.http_method, &http_url, &http_req, recv_msg, 512, 1000);
                        aos_free(recv_msg);
                    }
                    recv_msg = NULL;
                    // mars_http_getpost(http_msg.http_method, &http_url, &http_req, NULL, 0, 1000);
                    aos_free(http_req.content);
                }
                http_req.content = NULL;
            }

            aos_msleep(100);
        }else{
            aos_msleep(50);
        }
    }
}

char *mars_cloud_tokenget(void){
    return g_cloud_token;
}

char *mars_cloud_logsignget(void){
    return g_log_sign;
}

static aos_task_t task_http_process;
void mars_cloud_init(void)
{
    aos_task_new_ext(&task_http_process, "http process", http_process_task, NULL, (2*1024), AOS_DEFAULT_APP_PRI);
    init_http_queue();

    char tmp[5] = {0};
    char b_tokenstr[50] = "device";         //token string before md5
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();
    strcat(b_tokenstr, mars_template_ctx->device_name);
    uint8_t a_token[16] = {0};          //token string after md5
    mbedtls_md5(b_tokenstr, strlen(b_tokenstr), a_token);
    
    memset(g_cloud_token, 0, sizeof(g_cloud_token));
    for(int i=0;i<16;++i){
        sprintf(tmp, "%02x", a_token[i]);
        strcat(g_cloud_token, tmp);
    }
    
#if 0
    char b_signstr[80] = "1643338882";      //sign string before md5
    strcat(b_signstr, DEV_CATEGORY);
    strcat(b_signstr, DEV_MODEL);
    strcat(b_signstr, "mars");
    uint8_t a_signstr[16] = {0};          //sign string after md5
    mbedtls_md5(b_signstr, strlen(b_signstr), a_signstr);
    strcat(g_log_sign, "1643338882.");
    for(int i=0;i<16;++i){
        sprintf(tmp, "%02x", a_signstr[i]);
        strcat(g_log_sign, tmp);
    }

#else
    strncpy(g_log_sign, "1643338882.c0d5349776f08ede54c7f241cea2a1d3", sizeof(g_log_sign));  //根据协议，sign直接用固定字符串
#endif
}