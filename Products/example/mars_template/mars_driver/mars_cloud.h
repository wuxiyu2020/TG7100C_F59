/*** 
 * @Author: zhoubw
 * @Date: 2022-03-11 11:45:44
 * @LastEditors: zhoubw
 * @LastEditTime: 2022-03-11 11:48:01
 * @FilePath: /ali-smartliving-device-alios-things/Products/example/mars_template/service/mars_cloud.h
 * @Description: 
 */

#ifndef __MARS_CLOUD_H__
#define __MARD_CLOUD_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>

#include "mars_httpc.h"
#include "../mars_devfunc/mars_devmgr.h"
#define SPEC_SEQ "123456@1578666696145"

//#define M_CLOUD_LOG_UARTHEAD    "[%ld]["
//#define M_CLOUD_LOG_UARTTAIL    "]\r\n\0"
//#define M_CLOUD_LOG_UARTTAIL    "|\r\n\0"

#define M_CLOUD_LOG_UARTHEAD    "||%ld|1|"
#define M_CLOUD_LOG_WIFI        "||%ld|2|"


//正式环境
// #define URL_TEMPERPOST          "http://mcook.marssenger.com/menu-anon/addCookCurve"            //温度曲线上报
// #define URL_COOKHISTORYPOST     "http://mcook.marssenger.com/menu-anon/addCookHistory"          //烹饪历史
// #define URL_LOGPOST             "http://mcook.iotmars.com/mlog/upload/device"                   //日志上报
// #define URL_FACTORYPOST         "http://192.168.101.199:63136/iot/push/testing/result"          //产测地址

//测试环境
#define URL_TEMPERPOST       "http://mcook.dev.marssenger.net/menu-anon/addCookCurve"
#define URL_COOKHISTORYPOST  "http://mcook.dev.marssenger.net/menu-anon/addCookHistory"
#define URL_LOGPOST          "http://mcook.dev.marssenger.net/mlog/upload/device"
#define URL_FACTORYPOST      "http://192.168.101.199:63036/iot/push/testing/result"

// 本地测试地址
// #define URL_COOKHISTORYPOST  "192.168.124.24:12300"
// #define URL_TEMPERPOST       "192.168.1.137:12300"
// #define URL_LOGPOST          "192.168.1.137:12300"
// #define URL_FACTORYPOST      "192.168.213.19:12300"

#pragma pack(1)
typedef struct _PROPERTY_REPORT_MSG {
    char seq[24];
    uint32_t flag;
} property_report_msg_t;

typedef struct _RECV_MSG{
    int flag;
    uint8_t method;
    char seq[24];
    uint8_t from;
} recv_msg_t;

typedef struct _HTTP_MSG{
    uint8_t http_method;
    char *msg_str;
    int msg_len;
} http_msg_t;
#pragma pack(0)

enum HTTP_METHOD{
    HTTP_DEFAULT,
    HTTP_LOGPOST,       //日志上报
    HTTP_TEMPCURVE,     //温度曲线
    HTTP_COOKHISTORY,
    // HTTP_WEATHER,       //天气获取
};

int64_t mars_getTimestamp_ms(void);
void mars_store_time_date(void);

void report_device_property(char *seq, int flag);

void get_cloud_time(void *arg1, void *arg2);
void get_cloud_time_timer_start(void);

int32_t user_post_event_json(device_event_t id);
int32_t user_post_property_json(const char *property);

void  mars_cloud_init(void);
char *mars_cloud_tokenget(void);
char *mars_cloud_logsignget(void);
void send_http_to_queue(http_msg_t * http_msg);
int  mars_http_getpost(uint8_t method, const char *http_url, httpc_req_t *http_req, char *msg_out, uint16_t out_len, uint32_t timeout);
void mars_cloud_logpost(char *log_msg, int log_len);

#ifdef __cplusplus
}
#endif

#endif
