/*
 * @Author: zhoubw
 * @Date: 2022-03-11 11:40:45
 * @LastEditors  : zhoubw
 * @LastEditTime : 2022-11-28 09:51:02
 * @FilePath     : /tg7100c/Products/example/mars_template/mars_devfunc/mars_factory.c
 * @Description: 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>

#include <aos/aos.h>
#include <hal/wifi.h>
#include "netmgr.h"

#include "app_entry.h"
#include "mars_httpc.h"

#include "mars_factory.h"
#include "mars_devmgr.h"
#include "../mars_driver/mars_network.h"

#include "iot_export.h"
#include "iot_export_awss.h"
#include "../mars_driver/mars_cloud.h"

// #include "iotx_ota_internal.h"

uint8_t g_factory_status = 0;

int8_t g_factory_signalStrength = -100;


static void wifi_disconnect()
{
	wifi_mgmr_sta_disconnect();
	aos_msleep(1000);
	wifi_mgmr_sta_disable(NULL);
}

uint8_t mars_factory_status(void)
{
    return g_factory_status;
}

#define FACTORY_TEST_AP_SSID "moduletest"
#define FACTORY_TEST_AP_PSW  "58185818"
// #define FACTORY_TEST_AP_SSID "IoT-Test"
// #define FACTORY_TEST_AP_PSW  "12345678"

/* 测试信号强度 */
#define SCAN_STATE_IDLE         (0)
#define SCAN_STATE_BUSY         (1)
#define SCAN_STATE_DONE         (2)
static int test_scan_state = SCAN_STATE_IDLE;
static void scan_done_cb(void *data, void *param) 
{
  test_scan_state = SCAN_STATE_DONE;
}

void mars_factory_wifiTest(void)
{
    uint8_t buf_setmsg[UARTMSG_BUFSIZE] = {0};
    uint16_t buf_len = 0;
    M_CMD_TYPE_E buf_cmd = cmd_null;
    
    ap_scan_info_t scan_result = {0};
    int ap_scan_result = -1;
    buf_setmsg[buf_len++] = prop_FactoryResult;
    buf_setmsg[buf_len++] = 0x01;
    buf_cmd = cmd_store;
    // start ap scanning for default 3 seconds
    memset(&scan_result, 0, sizeof(ap_scan_info_t));

    //老版本扫描信号强度
#if 0
    LOGI("mars", "产测: 开始扫描热点 %s \r\n", FACTORY_TEST_AP_SSID);
    ap_scan_result = awss_apscan_process(NULL, FACTORY_TEST_AP_SSID, &scan_result);
    LOGI("mars", "ap_scan_result====%d, scan_result.found===%d\n",ap_scan_result,scan_result.found);
    if ( (ap_scan_result == 0) && (scan_result.found) )
    {
        g_factory_signalStrength = scan_result.rssi;
        LOGI("mars", "产测: 热点%s扫描成功: 信号强度=%d\r\n",FACTORY_TEST_AP_SSID, g_factory_signalStrength);
	    buf_setmsg[1] = 0x00;
    	aos_post_event(EV_DEVFUNC, MARS_EVENT_FACTORY_WASS, 0);      //normal, send normal in mars_factory_linkkey function
    }
    else
    {
    	Mars_uartmsg_send(buf_cmd, uart_get_seq_mid(), buf_setmsg, buf_len, 3);   //Scan No Find ,send abnormal to uart
        LOGI("mars", "产测: 产测失败!!!(moduletest扫描失败)\r\n");
    }  
#endif

    //新版本扫描信号强度 
    LOGI("mars", "产测: 开始扫描热点 %s \r\n", FACTORY_TEST_AP_SSID);
    wifi_mgmr_scan(NULL, scan_done_cb);//trigger the scan
    while (SCAN_STATE_DONE != test_scan_state) 
    {
        LOGW("mars", "wifi_scan: 扫描中......");
        aos_msleep(1000);
    }

    wifi_mgmr_ap_item_t ap_itme = {0x00};
    if (wifi_mgmr_scan_ap(FACTORY_TEST_AP_SSID, &ap_itme) == 0)
    {
        g_factory_signalStrength = ap_itme.rssi;
        LOGI("mars", "产测: %s扫描成功: 信号强度=%d dBm",FACTORY_TEST_AP_SSID, g_factory_signalStrength);
	    buf_setmsg[1] = 0x00;
    	aos_post_event(EV_DEVFUNC, MARS_EVENT_FACTORY_WASS, 0);      //normal, send normal in mars_factory_linkkey function
    }
    else
    {
        Mars_uartmsg_send(buf_cmd, uart_get_seq_mid(), buf_setmsg, buf_len, 3);   //Scan No Find ,send abnormal to uart
        LOGE("mars", "产测: %s扫描失败!!!!!!", FACTORY_TEST_AP_SSID);
    }
    test_scan_state = SCAN_STATE_IDLE;
}

void mars_factory_awss(void)
{
    M_CMD_TYPE_E buf_cmd = cmd_null;
    //wifi_disconnect();
    //aos_msleep(10*1000);
    g_factory_status = 1;
    netmgr_ap_config_t config = {0};
    strncpy(config.ssid, FACTORY_TEST_AP_SSID, sizeof(config.ssid) - 1);
    strncpy(config.pwd,  FACTORY_TEST_AP_PSW,  sizeof(config.pwd)  - 1);
    netmgr_set_ap_config(&config);
    LOGI("mars", "产测: 开始连接热点 (热点=%s 密码=%s)", FACTORY_TEST_AP_SSID, FACTORY_TEST_AP_PSW);
    netmgr_start(false);
}

#define FACTORYNUMBER_TAG     "\"data\":"
void factory_number_process(char *msg, uint16_t msg_len)
{
    if (NULL == msg){
        return;
    }

    uint8_t factorynum = 0;
    char *num_head = strstr(msg, FACTORYNUMBER_TAG);
    if (num_head){
        char *num_tail = strstr(num_head, ",");
        if (!num_tail){
            num_tail = strstr(num_head, "}");
        }

        if (num_tail){
            char num_str[10] = {0};
            memcpy(num_str, num_head+strlen(FACTORYNUMBER_TAG), num_tail-num_head-strlen(FACTORYNUMBER_TAG));
            factorynum = atoi(num_str);
        } 
    }

    uint8_t buf[10] = {0};
    uint16_t len = 0;
    buf[len++] = 0xE6;          //产测序号
    buf[len++] = factorynum; 
    Mars_uartmsg_send(cmd_set, uart_get_seq_mid(), &buf, len, 3);

    return;
}

// extern void utils_md5(const unsigned char *input, size_t ilen, unsigned char output[16]);
#define STR_FACTORYPOST  "{\"signalStrength\":\"%d\",\"quadruple\":true,\"versionCommunicationPanel\":\"%s\", \
\"versionPowerPanel\":\"%s\",\"versionDisplayPanel\":\"%s\", \"versionHeader\":\"%s\", \
\"deviceName\":\"%s\",\"deviceSecret\":\"%s\",\"mqVerify\":\"%s\"}"
extern void mbedtls_md5(const unsigned char *input, size_t ilen, unsigned char output[16]);

void mars_factory_linkkey(void)
{
    g_factory_status = 0;			//clear factory status
    
    uint8_t buf_msg[UARTMSG_BUFSIZE] = {0};
    // uint16_t buf_len = 0;
    int ret = -1;
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();

    if (0 == strlen(mars_template_ctx->product_key) || 0 == strlen(mars_template_ctx->product_secret)
        || 0 == strlen(mars_template_ctx->device_name) || 0 == strlen(mars_template_ctx->device_secret)){   //四元组缺失
        buf_msg[0] = prop_FactoryResult;
        buf_msg[1] = 0x05;
        Mars_uartmsg_send(cmd_store, uart_get_seq_mid(), buf_msg, 2, 3);
        LOGI("mars", "产测: 产测失败!!!(四元组缺失)\r\n");
        return;
    }else{
        buf_msg[0] = prop_FactoryResult;
        buf_msg[1] = 0x00;
        Mars_uartmsg_send(cmd_store, uart_get_seq_mid(), buf_msg, 2, 3);
        LOGI("mars", "产测: 产测成功!!!\r\n");
    }

    char http_url[] = URL_FACTORYPOST;
    
    char before_md5[200] = {0};
    uint8_t after_md5[17] = {0};
    strcat(before_md5, "1643338882000");
    strcat(before_md5, mars_template_ctx->product_key);
    strcat(before_md5, mars_template_ctx->macStr);
    strcat(before_md5, "mars");
    
    mbedtls_md5((unsigned char *)before_md5, strlen(before_md5), after_md5);
    uint8_t str_sign[80] = {0};
    strcat(str_sign, "1643338882000.");
    char md5_str[35] = {0};
    for (int i=0; i<16; i++){
        char tmp_str[5] = {0};
        sprintf(tmp_str, "%02x", after_md5[i]);
        strcat(md5_str, tmp_str);
    }
    strcat(str_sign, md5_str);
    LOGI("mars", "bmd5:%s amd5:%s", before_md5, str_sign);

    bzero(before_md5, sizeof(before_md5));
    bzero(after_md5, sizeof(after_md5));
    strcat(before_md5, mars_template_ctx->device_name);
    strcat(before_md5, mars_template_ctx->device_secret);
    strcat(before_md5, mars_template_ctx->product_key);
    strcat(before_md5, mars_template_ctx->product_secret);
    
    mbedtls_md5((unsigned char *)before_md5, strlen(before_md5), after_md5);

    char str_mqVerify[35] = {0};
    for (int i=0; i<16; i++){
        char tmp_str[5] = {0};
        sprintf(tmp_str, "%02x", after_md5[i]);
        strcat(str_mqVerify, tmp_str);
    }
    LOGI("mars", "bmd5:%s amd5:%s", before_md5, str_mqVerify);
    //为加快处理速度，使用字符串打印组件json包
    char buf_str[512] = {0};
    char pwrver_str[8] = {0};
    sprintf(pwrver_str, "%X.%X", \
            (uint8_t)(mars_template_ctx->status.PwrSWVersion >> 4), \
            (uint8_t)(mars_template_ctx->status.PwrSWVersion & 0x0F));
    char ctrlver_str[8] = {0};
    sprintf(ctrlver_str, "%X.%X", \
            (uint8_t)(mars_template_ctx->status.ElcSWVersion >> 4), \
            (uint8_t)(mars_template_ctx->status.ElcSWVersion & 0x0F));
    char colorver_str[8] = {0};
    sprintf(colorver_str, "%X.%X", \
            (uint8_t)(mars_template_ctx->status.DisplaySWVersion >> 4), \
            (uint8_t)(mars_template_ctx->status.DisplaySWVersion & 0x0F));
    sprintf(buf_str, STR_FACTORYPOST, \
            g_factory_signalStrength, \
            aos_get_app_version(), \
            pwrver_str, \
            ctrlver_str, \
            colorver_str, \
            mars_template_ctx->device_name, \
            mars_template_ctx->device_secret, \
            str_mqVerify);

    http_session_t http_handle = 0;
    parsed_url_t t_url = {0};
    httpc_req_t http_req = {
        .type = HTTP_POST,
        .version = HTTP_VER_1_1,
        .resource = NULL,
        .content = NULL,
        .content_len = 0,
    };

    int read_size = 0;

    char *tmpbuf = (char *)aos_malloc(256);
    //LOGI("mars", "mars_factory_linkkey: aos_malloc = 0x%08X", tmpbuf);
    if (tmpbuf){
        bzero(tmpbuf, 256);
        ret = http_parse_URL(http_url, tmpbuf, 256, &t_url);
        if (0 == ret){
            ret = http_open_session(&http_handle, http_url, 0, NULL, 3);
            if (0 == ret){
                http_req.content = buf_str;
                http_req.content_len = strlen(buf_str);
                http_req.resource = t_url.resource;            
                ret = http_prepare_req(http_handle, &http_req, HDR_ADD_CONN_CLOSE);
                if (0 == ret){
                    http_add_header(http_handle, "Content-Type", "application/json");
                    http_add_header(http_handle, "mac", mars_template_ctx->macStr);
                    http_add_header(http_handle, "pk", mars_template_ctx->product_key);
                    http_add_header(http_handle, "sign",str_sign);
                    ret = http_send_request(http_handle, &http_req);

                    if (0 == ret){
                        int out_len = 256;
                        char *msg_out = (char *)malloc(out_len);
                        if (msg_out){
                            bzero(msg_out, out_len);
                            read_size = http_read_content_timeout(http_handle, msg_out, out_len, 3000);
                            if (read_size != 0){
                                LOGI("mars", "产测 http上报: %s", buf_str);
                                LOGI("mars", "产测 http接收: %s", msg_out);
                                factory_number_process(msg_out, out_len);
                                netmgr_clear_ap_config();
                            #ifdef EN_COMBO_NET
                                breeze_clear_bind_info();
                            #endif
                                // do_awss_reset();
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
        //LOGI("mars", "mars_factory_linkkey: aos_free = 0x%08X", tmpbuf);
        tmpbuf = NULL;  
    }else{
        tmpbuf = NULL;  
        mprintf_e("malloc failed.");
    }

    LOGI("mars", "产测完成,即将复位......");
    aos_msleep(2000);
    aos_reboot();  //do_awss_reset
}

void mars_factory_uartMsgFromSlave(uint8_t cmd, mars_template_ctx_t *mars_template_ctx)
{
    switch(cmd)
    {
        case 0x01:
        {
            LOGI("mars", "收到电控板上电通知"); 
            mars_store_time_date();        
            mars_store_netstatus();
            mars_store_voicestatus();
            mars_devmngr_getstatus(NULL, NULL);   
          //mars_store_swversion();
            break;
        }

        case 0x02:
        {
            // reboot wifi

            break;
        }

        case 0x03:  //智能化厂测开始
        {
            LOGI("mars", "收到指令: 智能化厂测开始");
	        hal_wifi_suspend_station(NULL); //wifi_disconnect();
            aos_post_event(EV_DEVFUNC, MARS_EVENT_SEND_SWVERSION, 0);
	        break;
        }

        case 0x04:  //智能化厂测结束
        {
            LOGI("mars", "收到指令: 智能化厂测结束");
            break;
        }

        case 0x06:
        {
            LOGI("mars", "收到指令: 恢复出厂");
            aos_msleep(1000);
            uint8_t buf[] = {prop_FactoryResult, 0x04};
            Mars_uartmsg_send(cmd_store, uart_get_seq_mid(),  buf, sizeof(buf), 3);
            LOGI("mars", "发送恢复出厂成功......");
            do_awss_reset();
            break;
        }

        case 0xA1:
        {
            LOGI("mars", "收到错误指令: error 0xF1-0xA1");
            break;
        }

        case 0xA4:
        {
            LOGI("mars", "收到错误指令: error 0xF1-0xA4");
            break;
        }

        case 0xB1:
        {
            LOGI("mars", "收到错误指令: error 0xF1-0xB1");            
            break;
        }

        default:
            break;
    }
}


