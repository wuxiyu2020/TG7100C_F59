/*
 * @Description  : 
 * @Author       : zhoubw
 * @Date         : 2022-06-06 17:18:46
 * @LastEditors  : zhoubw
 * @LastEditTime : 2022-11-29 13:55:33
 * @FilePath     : /tg7100c/Products/example/mars_template/mars_devfunc/mars_devmgr.c
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <cJSON.h>

#include "../dev_config.h"
#include "app_entry.h"

#include "../mars_driver/mars_uartmsg.h"
#include "../mars_driver/mars_cloud.h"

#include "mars_devfunc/irt102m/iot_voicectrl.h"

#include "mars_devmgr.h"

#if MARS_STOVE
#include "mars_stove.h"
#endif

#if MARS_STEAMOVEN
#include "mars_steamoven.h"
#endif
#if MARS_DISHWASHER
#include "mars_dishwasher.h"
#endif
#if MARS_STERILIZER

#include "mars_display.h"

#include "mars_factory.h"
#endif

#define         VALID_BIT(n)            ((uint16_t)1 << (n - prop_ElcSWVersion))
static mars_template_ctx_t g_user_example_ctx = {0};
static aos_timer_t period_query_timer;
int mars_ble_awss_state = 0;
int g_voice_switch = 1;
int g_volume_gear  = SET_VOLUME_MAX;

mars_template_ctx_t *mars_dm_get_ctx(void)
{
    return &g_user_example_ctx;
}

void mars_beer_control()
{
    uint8_t buf[] = {prop_Beer, 1};
    Mars_uartmsg_send(cmd_set, uart_get_seq_mid(),  buf, sizeof(buf), 3);
    LOGI("mars", "下发蜂鸣器: 短鸣一声");
}

void mars_store_netstatus()
{
    char* net_des[] = {"未联网", "已联网", "配网中"};
    uint8_t buff[] = {prop_NetState, mars_dm_get_ctx()->status.NetState};
    Mars_uartmsg_send(cmd_store, uart_get_seq_mid(),  buff, sizeof(buff), 3);
    LOGI("mars", "下发网络状态: %s", net_des[mars_dm_get_ctx()->status.NetState]);
}

void mars_store_voicestatus()
{
    char* voice_des[] = {"语音关闭", "语音开启"};
    uint8_t buff[] = {prop_VoiceSwitchState, g_voice_switch};
    Mars_uartmsg_send(cmd_store, uart_get_seq_mid(),  buff, sizeof(buff), 3);
    LOGI("mars", "下发语音状态: %s", voice_des[g_voice_switch]);
}

void mars_store_swversion(void)
{
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();
    uint8_t buf[] = {prop_NetFwVer, mars_template_ctx->status.WifiSWVersion[0]};
    // buf[0] = prop_NetFwVer;
    // buf[1] = mars_template_ctx->status.WifiSWVersion[0];
    // buf[2] = mars_template_ctx->status.WifiSWVersion[1];
    // buf[3] = mars_template_ctx->status.WifiSWVersion[2];
    Mars_uartmsg_send(cmd_store, uart_get_seq_mid(), &buf, sizeof(buf), 3);
    LOGI("mars", "下发通讯板软件版本: 0x%02X", mars_template_ctx->status.WifiSWVersion[0]);
}

void mars_devmngr_getstatus(void *arg1, void *arg2)
{    
    uint8_t buf[] = {0};
    Mars_uartmsg_send(cmd_get, uart_get_seq_mid(),  &buf, 1, 3);
    LOGI("mars", "请求电控板全属性");
}

bool mars_uart_prop_process(uartmsg_que_t *msg)
{
    if (cmd_keypress == msg->cmd)
    {
        Mars_uartmsg_send(cmd_ack, msg->seq, NULL, 0, 0);
    }
    else if (cmd_event == msg->cmd)
    {
        Mars_uartmsg_send(cmd_ack, msg->seq, NULL, 0, 0);
    }
    else if (cmd_getack == msg->cmd)
    {
        mars_uartmsg_ackseq_by_seq(msg->seq);
    }
    else
    {
        LOGE("mars", "未知命令码");
    }

    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();//烹饪助手温度上报消息直接处理 
    uint8_t nak    = 0;
    bool report_en = false;
    for (uint16_t i=0; i<msg->len; ++i)
    {
        //LOGW("mars", "开始解析属性 0x%02X", msg->msg_buf[i]);
        if (msg->msg_buf[i] >= PROP_SYS_BEGIN && msg->msg_buf[i] <= PROP_SYS_END)
        {
            switch (msg->msg_buf[i])
            {
                case prop_ElcSWVersion:
                {
                    if (mars_template_ctx->status.ElcSWVersion != msg->msg_buf[i+1])
                    {
                        mars_template_ctx->status.ElcSWVersion = msg->msg_buf[i+1];
                        extern bool mars_ota_inited(void);
                        if (mars_ota_inited())
                        {
                           mars_ota_module_1_init();
                        }

                        extern void del_dis_fw_proc(uint8_t ver);
                        LOGW("mars", "开始删除显示板固件");
                        del_dis_fw_proc(mars_template_ctx->status.ElcSWVersion);
                    }                    
                    mars_template_ctx->common_reportflg |= VALID_BIT(msg->msg_buf[(i)]);
                    report_en = true;
                    i+=1;
                    break;
                }
                case prop_ElcHWVersion:
                {
                    mars_template_ctx->status.ElcHWVersion = msg->msg_buf[i+1];
                    mars_template_ctx->common_reportflg |= VALID_BIT(msg->msg_buf[(i)]);
                    report_en = true;
                    i+=1;
                    break;
                }
                case prop_PwrSWVersion:
                {
                    if (mars_template_ctx->status.PwrSWVersion != msg->msg_buf[i+1])
                    {
                        mars_template_ctx->status.PwrSWVersion = msg->msg_buf[i+1];

                        extern void del_pwr_fw_proc(uint8_t ver);
                        LOGW("mars", "开始删除电源板固件");
                        del_pwr_fw_proc(mars_template_ctx->status.PwrSWVersion);
                    }
                    mars_template_ctx->common_reportflg |= VALID_BIT(msg->msg_buf[(i)]);
                    report_en = true;
                    i+=1;
                    break;
                }
                case prop_PwrHWVersion:
                {
                    mars_template_ctx->status.PwrHWVersion = msg->msg_buf[i+1];
                    mars_template_ctx->common_reportflg |= VALID_BIT(msg->msg_buf[(i)]);
                    report_en = true;
                    i+=1;
                    break;
                }
                case prop_SysPower:
                {
                    if(0x00 == msg->msg_buf[i+1] || 0x01 == msg->msg_buf[i+1])
                    {
                        mars_template_ctx->status.SysPower = msg->msg_buf[i+1];                        
                        if (mars_template_ctx->status.SysPower == 0x00)//判断是否要ota重启
                        {
                            extern void mars_ota_reboot(void);
                            mars_ota_reboot();
                        }
                        mars_template_ctx->common_reportflg |= VALID_BIT(msg->msg_buf[(i)]);
                        report_en = true;
                    }
                    i+=1;
                    break;
                }
                case prop_Netawss:
                {
                    if(msg->msg_buf[i+1] == 0x01)
                    {
                        LOGW("mars", "串口命令: 启动蓝牙配网");
                        if (!is_awss_state())
                        {
                            mars_dm_get_ctx()->status.NetState = NET_STATE_CONNECTING;
                            mars_store_netstatus();
                            del_bind_flag();
                            do_awss_reset();
                            do_awss_reboot();
                        }
                        else
                        {
                            LOGE("mars", "已处于配网,本命令忽略");
                        }
                    }
                    else if(msg->msg_buf[i+1] == 0x00)
                    {
                        extern void timer_func_awss_finish(void *arg1, void *arg2);
                        LOGW("mars", "串口命令: 退出蓝牙配网");
                        if (is_awss_state())
                            timer_func_awss_finish(NULL, NULL);                        
                        else                        
                            LOGE("mars", "未处于配网,本命令忽略 error");                        
                    }
                    i+=1;
                    break;
                }
                case prop_ErrCode:
                {
                    mars_template_ctx->status.ErrCode = (uint32_t)(msg->msg_buf[i+1] << 24) | (uint32_t)(msg->msg_buf[i+2] << 16) |  (uint32_t)(msg->msg_buf[i+3] << 8)  | (uint32_t)(msg->msg_buf[i+4]);
                    LOGI("mars", "警报状态ErrCode: 0x%08X", mars_template_ctx->status.ErrCode);
                    mars_template_ctx->common_reportflg |= VALID_BIT(msg->msg_buf[(i)]);
                    report_en = true;
                    i+=4;
                    break;
                }
                case prop_ErrCodeShow:
                {
                    if (msg->msg_buf[i+1] != mars_template_ctx->status.ErrCodeShow)
                    {
                        if (msg->msg_buf[i+1] != 0)
                        {
                            user_post_event_json(msg->msg_buf[i+1]);
                            LOGE("mars", "推送事件: 故障发生推送 (历史值=%d 最新值=%d)", mars_template_ctx->status.ErrCodeShow, msg->msg_buf[i+1]);

                            if (msg->msg_buf[i+1] == 2)
                            {
                                LOGI("mars", "播报: 水箱未放置到位，请推至最深处");
                                xm_open_voice_file("142");
                            }
                            else if (msg->msg_buf[i+1] == 3)
                            {
                                LOGI("mars", "播报: 主人，水箱缺水，请加水");
                                xm_open_voice_file("141");
                            }
                        }
                        else
                        {
                            user_post_event_json(msg->msg_buf[i+1]);
                            LOGI("mars", "推送事件: 故障恢复推送 (历史值=%d 最新值=%d)", mars_template_ctx->status.ErrCodeShow, msg->msg_buf[i+1]);
                        }
                    }
                    mars_template_ctx->status.ErrCodeShow = msg->msg_buf[i+1];
                    mars_template_ctx->common_reportflg |= VALID_BIT(msg->msg_buf[(i)]);
                    report_en = true;
                    i+=1;
                    break;
                }
                default:
                    LOGE("mars", "error 收到未知属性,停止解析!!! (属性 = 0x%02X)", msg->msg_buf[i]);
                    return false;
                    break;
            }
        }
        else if (msg->msg_buf[i] >= PROP_INTEGRATED_STOVE_BEIGN && msg->msg_buf[i] <= PROP_INTEGRATED_STOVE_END)
        {
            mars_stove_uartMsgFromSlave(msg, mars_template_ctx, &i, &report_en, &nak);
        }
        else if (msg->msg_buf[i] >= PROP_PARA_BEGIN && msg->msg_buf[i] <= PROP_PARA_END)
        {
            mars_steamoven_uartMsgFromSlave(msg, mars_template_ctx, &i, &report_en, &nak);
        }
        else if (msg->msg_buf[i] == prop_LSteamGear) //0x80
        {
            #define  STOV_SPEC_VALID_BIT(n)  ((uint16_t)1 << (n - prop_LSteamGear))
            mars_template_ctx->status.LSteamGear = msg->msg_buf[i+1];
            mars_template_ctx->steamoven_spec_reportflg |= STOV_SPEC_VALID_BIT(msg->msg_buf[(i)]);
            report_en = true;
            i+=1;
            LOGI("mars", "收到蒸汽档位 (蒸汽档位 = %d)", msg->msg_buf[i+1]);
        }
        else if (msg->msg_buf[i] >= prop_ChickenSoupDispSwitch && msg->msg_buf[i] <= prop_DisplayHWVersion)
        {
            mars_display_uartMsgFromSlave(msg, mars_template_ctx, &i, &report_en, &nak);
        }
        else if(msg->msg_buf[i] >= PROP_SPECIAL_BEGIN && msg->msg_buf[i] <= PROP_SPECIAL_END)//special property begin
        {
            switch(msg->msg_buf[i])
            {
                case prop_LocalAction:
                {
                    mars_factory_uartMsgFromSlave(msg->msg_buf[i+1], mars_template_ctx);
                    i+=1;
                    break;
                }
                case prop_DataReportReason:
                {
                    if(cmd_getack == msg->cmd || cmd_event == msg->cmd)
                    {
                        mars_template_ctx->status.DataReportReason = msg->msg_buf[i+1];                        
                    }
                    else
                    {
                        nak = NAK_ERROR_CMDCODE_NOSUPPORT;
                    }
                    i+=1;
                    break;
                }
                default:
                    LOGE("mars", "error 收到未知属性,停止解析!!! (属性 = 0x%02X)", msg->msg_buf[i]);
                    return false;
                    break;
            }
        }
        else if(msg->msg_buf[i] >= PROP_CTRL_LOG_BEGIN && msg->msg_buf[i] <= PROP_CTRL_LOG_END)
        {
            //LOGW("mars", "收到日志上报属性: 0x%02X", msg->msg_buf[i]);
            if (msg->msg_buf[i] == 0xC0)
                i+=2;
            else if (msg->msg_buf[i] == 0xC1)
                i+=2;
            else if (msg->msg_buf[i] == 0xC2)
                i+=2;
            else if (msg->msg_buf[i] == 0xC3)
                i+=15;
            else if (msg->msg_buf[i] == 0xC4)
                i+=1;
            else if (msg->msg_buf[i] == 0xC5)
                i+=1;
            else if (msg->msg_buf[i] == 0xC6)
                i+=2;
            else if (msg->msg_buf[i] == 0xC7)
                i+=2;            
            else if (msg->msg_buf[i] == 0xC8)
                i+=12;
        }
        else if (msg->msg_buf[i] == 0xA0)
        {
            if (msg->msg_buf[i+1] == 0x00)
            {
                LOGI("mars", "播报: 语音已关闭");
                xm_open_voice_file("140");
                aos_msleep(1500);
                g_voice_switch = 0;
                xm_voice_set_volume(SET_SPAKER_MUTE);
                report_wifi_property(NULL, NULL);
                mars_store_voicestatus();
            }
            else
            {               
                g_voice_switch = 1;
                xm_voice_set_volume(SET_SPAKER_UNMUTE);
                aos_msleep(500);
                LOGI("mars", "播报: 语音已开启");
                xm_open_voice_file("139");
                report_wifi_property(NULL, NULL);
                mars_store_voicestatus();
            }
            i+=1;
        }
        else
        {
            LOGE("mars", "error 收到未知属性,停止解析!!! (属性 = 0x%02X)", msg->msg_buf[i]);
            report_en = false;
            break;
        }  
    }

    return report_en;
}

int Mars_property_set_callback(cJSON *root, cJSON *item, void *msg)
{
    if (NULL == root || NULL == msg){
        return -1;
    }

    int ret = 0;
    uint8_t buf_setmsg[UARTMSG_BUFSIZE] = {0};
    uint16_t buf_len = 0;
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();

    if ((item = cJSON_GetObjectItem(root, "SysPower")) != NULL && cJSON_IsNumber(item)) 
    {
        mars_template_ctx->status.SysPower = item->valueint;
        buf_setmsg[buf_len++] = prop_SysPower;
        buf_setmsg[buf_len++] = mars_template_ctx->status.SysPower;
    }

    if ((item = cJSON_GetObjectItem(root, "OtaCmdPushType")) != NULL && cJSON_IsNumber(item)) 
    {
        mars_template_ctx->status.OTAbyAPP = item->valueint;
        LOGI("mars", "app下发升级类型: %d (0-静默升级 1-app触发)", item->valueint);
        //buf_setmsg[buf_len++] = prop_SysPower;
        //buf_setmsg[buf_len++] = mars_template_ctx->status.SysPower;
    }

    if ((item = cJSON_GetObjectItem(root, "VolumeGear")) != NULL && cJSON_IsNumber(item)) 
    {
        g_volume_gear = item->valueint;
        xm_voice_set_volume(item->valueint);
        LOGI("mars", "设置音量档位: %d", g_volume_gear);
        report_wifi_property(NULL, NULL);
    }

    if ((item = cJSON_GetObjectItem(root, "VoiceSwitch")) != NULL && cJSON_IsNumber(item)) 
    {
        if (item->valueint == 0)
        {
            g_voice_switch = 0;
            xm_voice_set_volume(SET_SPAKER_MUTE);
            LOGW("mars", "关闭语音");
            report_wifi_property(NULL, NULL);
            mars_store_voicestatus();
        }
        else
        {
            g_voice_switch = 1;
            xm_voice_set_volume(SET_SPAKER_UNMUTE);
            LOGI("mars", "打开语音");
            // aos_msleep(500);
            // xm_voice_set_volume(g_volume_gear);
            // LOGI("mars", "设置音量档位: %d", g_volume_gear);
            report_wifi_property(NULL, NULL);
            mars_store_voicestatus();
        }
    }


#if MARS_STOVE
    mars_stove_setToSlave(root, item, mars_template_ctx, buf_setmsg, &buf_len);
#endif

#if MARS_STEAMOVEN
    mars_steamoven_setToSlave(root, item, mars_template_ctx, buf_setmsg, &buf_len);
#endif

#if MARS_DISHWASHER
   mars_dishwasher_setToSlave(root, item, mars_template_ctx, buf_setmsg, &buf_len);
#endif //MARS_DISHWASHER

#if MARS_DISPLAY
    mars_display_setToSlave(root, item, mars_template_ctx, buf_setmsg, &buf_len);
#endif

    // if ((item = cJSON_GetObjectItem(root, "debug_data_current_date_time")) != NULL && cJSON_IsNumber(item)) {
    // }

    if (buf_len != 0)
    {
        Mars_uartmsg_send(cmd_set, uart_get_seq_mid(),  buf_setmsg, buf_len, 3);
    }

    return 0;
}

void Mars_property_get_callback(char *property_name, cJSON *response)
{
    if (NULL == property_name || NULL == response){
        return -1;
    }
    
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();

    if (strcmp("WifiMac", property_name) == 0)
    {
        cJSON_AddStringToObject(response, "WifiMac", mars_template_ctx->macStr);
    }
    else if (strcmp("SysPower", property_name) == 0) 
    {
        cJSON_AddNumberToObject(response, "SysPower", mars_template_ctx->status.SysPower);
    }
    else if (strcmp("ElcSWVersion", property_name) == 0)
    {
        char tmpstr[8] = {0};
        sprintf(tmpstr, "%X.%X", \
                (uint8_t)(mars_template_ctx->status.ElcSWVersion >> 4), \
                (uint8_t)(mars_template_ctx->status.ElcSWVersion & 0x0F));
        cJSON_AddStringToObject(response, "ElcSWVersion", tmpstr);
    }
    else if (strcmp("ElcHWVersion", property_name) == 0)
    {
        char tmpstr[8] = {0};
        sprintf(tmpstr, "%X.%X", \
                (uint8_t)(mars_template_ctx->status.ElcHWVersion >> 4), \
                (uint8_t)(mars_template_ctx->status.ElcHWVersion & 0x0F));
        cJSON_AddStringToObject(response, "ElcHWVersion", tmpstr);
    }
    else if (strcmp("PwrSWVersion", property_name) == 0)
    {
        char tmpstr[8] = {0};
        sprintf(tmpstr, "%X.%X", \
                (uint8_t)(mars_template_ctx->status.PwrSWVersion >> 4), \
                (uint8_t)(mars_template_ctx->status.PwrSWVersion & 0x0F));
        cJSON_AddStringToObject(response, "PwrSWVersion", tmpstr);
    }
    else if (strcmp("PwrHWVersion", property_name) == 0)
    {
        char tmpstr[8] = {0};
        sprintf(tmpstr, "%X.%X", \
                (uint8_t)(mars_template_ctx->status.PwrHWVersion >> 4), \
                (uint8_t)(mars_template_ctx->status.PwrHWVersion & 0x0F));
        cJSON_AddStringToObject(response, "PwrHWVersion", tmpstr);
    }
}

void mars_property_data(char* msg_seq, char **str_out)
{
    mars_template_ctx_t *t_mars_template_ctx = mars_dm_get_ctx();
    cJSON *proot = cJSON_CreateObject();
    if(NULL != proot)
    {
        //LOGI("mars", "mars_property_data: 1");
        if (NULL != msg_seq)
        {
            cJSON *item_csr = cJSON_CreateObject();
            if (item_csr == NULL) 
            {
                cJSON_Delete(proot);
                return;
            }
            cJSON_AddStringToObject(item_csr, "seq", msg_seq);
            cJSON_AddItemToObject(proot, "CommonServiceResponse", item_csr);
        }
        //LOGI("mars", "mars_property_data: 2");
        // cJSON_AddStringToObject(proot, "WifiMac", t_mars_template_ctx->macStr);

        if (t_mars_template_ctx->steamoven_spec_reportflg & STOV_SPEC_VALID_BIT(prop_LSteamGear))
        {
            LOGI("mars", "上报左腔蒸汽档位: %d", t_mars_template_ctx->status.LSteamGear);
            cJSON_AddNumberToObject(proot, "LSteamGear",  t_mars_template_ctx->status.LSteamGear);
        }
        t_mars_template_ctx->steamoven_spec_reportflg = 0;

        for (uint8_t index=prop_ElcSWVersion; index<=prop_ErrCodeShow; ++index)
        {
            if (t_mars_template_ctx->common_reportflg & VALID_BIT(index))
            {
                switch (index)
                {
                    case prop_ElcSWVersion:
                    {
                        char tmpstr[8] = {0};
                        sprintf(tmpstr, "%X.%X", \
                                (uint8_t)(t_mars_template_ctx->status.ElcSWVersion >> 4), \
                                (uint8_t)(t_mars_template_ctx->status.ElcSWVersion & 0x0F));
                        cJSON_AddStringToObject(proot, "ElcSWVersion", tmpstr);
                        break;
                    }
                    case prop_ElcHWVersion:
                    {
                        char tmpstr[8] = {0};
                        sprintf(tmpstr, "%X.%X", \
                                (uint8_t)(t_mars_template_ctx->status.ElcHWVersion >> 4), \
                                (uint8_t)(t_mars_template_ctx->status.ElcHWVersion & 0x0F));
                        cJSON_AddStringToObject(proot, "ElcHWVersion", tmpstr);
                        break;
                    }
                    case prop_PwrSWVersion:
                    {
                        char tmpstr[8] = {0};
                        sprintf(tmpstr, "%X.%X", \
                                (uint8_t)(t_mars_template_ctx->status.PwrSWVersion >> 4), \
                                (uint8_t)(t_mars_template_ctx->status.PwrSWVersion & 0x0F));
                        cJSON_AddStringToObject(proot, "PwrSWVersion", tmpstr);
                        break;
                    }
                    case prop_PwrHWVersion:
                    {
                        char tmpstr[8] = {0};
                        sprintf(tmpstr, "%X.%X", \
                                (uint8_t)(t_mars_template_ctx->status.PwrHWVersion >> 4), \
                                (uint8_t)(t_mars_template_ctx->status.PwrHWVersion & 0x0F));
                        cJSON_AddStringToObject(proot, "PwrHWVersion", tmpstr);
                        break;
                    }
                    case prop_SysPower:
                    {
                        LOGI("mars", "上报: 系统开关status.SysPower: %d", t_mars_template_ctx->status.SysPower);
                        cJSON_AddNumberToObject(proot, "SysPower", t_mars_template_ctx->status.SysPower);
                        break;
                    }
                    case prop_ErrCode:
                    {
                        LOGI("mars", "上报: 警报状态status.ErrCode: %d", t_mars_template_ctx->status.ErrCode);
                        cJSON_AddNumberToObject(proot, "ErrorCode", t_mars_template_ctx->status.ErrCode);
                        break;
                    }
                    case prop_ErrCodeShow:
                    {
                        LOGI("mars", "上报: 显示报警status.ErrorCodeShow: %d", t_mars_template_ctx->status.ErrCodeShow);
                        cJSON_AddNumberToObject(proot, "ErrorCodeShow", t_mars_template_ctx->status.ErrCodeShow);
                        break;
                    }

                    default:
                        break;
                }
            }
        }
        t_mars_template_ctx->common_reportflg = 0;
        
#if MARS_STOVE
        //LOGI("mars", "mars_property_data: 3");
        mars_stove_changeReport(proot, t_mars_template_ctx);
#endif

#if MARS_STEAMOVEN
        //LOGI("mars", "mars_property_data: 4");
        mars_steamoven_changeReport(proot, t_mars_template_ctx);
#endif

#if MARS_DISHWASHER
        mars_dishwasher_changeReport(proot, t_mars_template_ctx);
#endif

#if MARS_DISPLAY
        //LOGI("mars", "mars_property_data: 5");
        mars_display_changeReport(proot, t_mars_template_ctx);
#endif

        //LOGI("mars", "mars_property_data: 6");
        if(t_mars_template_ctx->status.DataReportReason != 0xFF)
        {
            cJSON_AddNumberToObject(proot, "DataReportReason", t_mars_template_ctx->status.DataReportReason);
        }
        t_mars_template_ctx->status.DataReportReason = 0xFF; 

        //LOGI("mars", "mars_property_data: 7");
        *str_out = cJSON_Print(proot); //cJSON_PrintUnformatted cJSON_Print
        if (*str_out  == NULL)
        {
            LOGI("mars", "错误: cJSON_Print() failed !!!");
            *str_out = cJSON_PrintUnformatted(proot);
            if (*str_out  == NULL)
            {
                LOGI("mars", "错误: cJSON_PrintUnformatted() failed !!!");
            }
        }
    }

    if(NULL != proot){
        cJSON_Delete(proot);
        proot = NULL;
    }
    //LOGI("mars", "mars_property_data: 8");
}

int mars_dev_event_callback(input_event_t *event_id, void *param)
{
    LOGI("mars", "产测收到子事件: code = %d (0:send swver  1:wifi scan  2:wifi connect  3:http report)", event_id->code);
	switch(event_id->code)
	{
        case MARS_EVENT_SEND_SWVERSION:
        {
            mars_store_swversion();
	        aos_msleep(500);
            aos_post_event(EV_DEVFUNC, MARS_EVENT_FACTORY_WIFITEST, 0);
            break;
        }
        case MARS_EVENT_FACTORY_WIFITEST:
        {
            mars_factory_wifiTest();
            break;
        }
        case MARS_EVENT_FACTORY_WASS:
        {
            mars_factory_awss();
            break;
        }
        case MARS_EVENT_FACTORY_LINKKEY:
        {
            mars_factory_linkkey();
            break;
        }
        case MARS_EVENT_FACTORY_RESET:
        {
            
            break;
        }
        case MARS_EVENT_REPORT_WIFI_PROPERTY:
        {
            extern void report_wifi_property(void *arg1, void *arg2);
            report_wifi_property(NULL, NULL);
            break;
        }
        default:
            break;
	}

	return 0;
}

void report_wifi_property(void *arg1, void *arg2)
{
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "ComSWVersion", aos_get_app_version());
    cJSON_AddStringToObject(root, "WifiMac",      g_user_example_ctx.macStr);
    cJSON_AddNumberToObject(root, "VoiceSwitch",  g_voice_switch);
    cJSON_AddNumberToObject(root, "VolumeGear",   g_volume_gear);

    char* str = cJSON_Print(root);
    if (str != NULL)
    {
        user_post_property_json(str);
        cJSON_free(str);
        str = NULL;
    }

    if (root != NULL)
    {
        cJSON_Delete(root);
        root = NULL;
    }        
}

#define MARS_PROPGET_INTERVAL     (5 * 60 * 1000)
static aos_timer_t mars_propget = {.hdl = NULL};
void report_property_timer_start(void)
{
    if (mars_propget.hdl == NULL)
    {
        LOGI("mars", "上报通讯板板当前状态 定时器启动......(周期=%d分钟)", MARS_PROPGET_INTERVAL/(60 * 1000));
        aos_timer_new_ext(&mars_propget, report_wifi_property, NULL, MARS_PROPGET_INTERVAL, 1, 1);
    }
}

void mars_devmgr_afterConnect(void)
{   
    mars_devmngr_getstatus(NULL, NULL);

    get_cloud_time(NULL, NULL);
    get_cloud_time_timer_start();

    report_wifi_property(NULL, NULL);
    report_property_timer_start();

    //get_mcook_weather();
    //get_mcook_weather_timer_start();
}

typedef void (*voice_callback_t)(char*);
typedef struct {
    char* en;
    char* cn;
    voice_callback_t cb;
}voice_en_cn_map;

void voice_handle_sys_on_off(char* data)
{
    mars_template_ctx_t *t_mars_template_ctx = mars_dm_get_ctx();
    if (strcmp(data, "deviceTurnOff") == 0)
    {
        LOGI("mars", "播报: 好的");
        xm_open_voice_file("107");
        uint8_t  buf_msg[] = {prop_SysPower, 0};
        Mars_uartmsg_send(cmd_set, uart_get_seq_mid(),  buf_msg, sizeof(buf_msg), 3);
    }
    else if (strcmp(data, "deviceTurnOn") == 0)
    {
        LOGI("mars", "播报: 好的");
        xm_open_voice_file("107");
        uint8_t  buf_msg[] = {prop_SysPower, 1};
        Mars_uartmsg_send(cmd_set, uart_get_seq_mid(), buf_msg, sizeof(buf_msg), 3);
    }
}

void voice_handle_hood_on_off(char* data)
{
    mars_template_ctx_t *t_mars_template_ctx = mars_dm_get_ctx();
    if (strcmp(data, "fanTurnOn") == 0)
    {
        LOGI("mars", "播报: 好的");
        xm_open_voice_file("107");
        uint8_t  buf_msg[] = {prop_SysPower, 1, prop_HoodSpeed, 2};
        Mars_uartmsg_send(cmd_set, uart_get_seq_mid(),  buf_msg, sizeof(buf_msg), 3);
    }
    else if (strcmp(data, "fanTurnOff") == 0)
    {
        LOGI("mars", "播报: 好的");
        xm_open_voice_file("107");
        uint8_t  buf_msg[] = {prop_HoodSpeed, 0};
        Mars_uartmsg_send(cmd_set, uart_get_seq_mid(),  buf_msg, sizeof(buf_msg), 3);
    }
    else if (strcmp(data, "fanStepOne") == 0)
    {
        LOGI("mars", "播报: 好的");
        xm_open_voice_file("107");
        uint8_t  buf_msg[] = {prop_SysPower, 1, prop_HoodSpeed, 1};
        Mars_uartmsg_send(cmd_set, uart_get_seq_mid(),  buf_msg, sizeof(buf_msg), 3);
    }
    else if (strcmp(data, "fanStepSec") == 0)
    {
        LOGI("mars", "播报: 好的");
        xm_open_voice_file("107");
        uint8_t  buf_msg[] = {prop_SysPower, 1, prop_HoodSpeed, 2};
        Mars_uartmsg_send(cmd_set, uart_get_seq_mid(),  buf_msg, sizeof(buf_msg), 3);
    }
    else if (strcmp(data, "fanStepThd") == 0)
    {
        LOGI("mars", "播报: 好的");
        xm_open_voice_file("107");
        uint8_t  buf_msg[] = {prop_SysPower, 1, prop_HoodSpeed, 3};
        Mars_uartmsg_send(cmd_set, uart_get_seq_mid(),  buf_msg, sizeof(buf_msg), 3);
    }
    else if (strcmp(data, "fanUp") == 0)
    {
        LOGI("mars", "播报: 好的");
        xm_open_voice_file("107");
        if (t_mars_template_ctx->status.HoodSpeed < 3)
        {
            uint8_t  buf_msg[] = {prop_SysPower, 1, prop_HoodSpeed, t_mars_template_ctx->status.HoodSpeed+1};
            Mars_uartmsg_send(cmd_set, uart_get_seq_mid(),  buf_msg, sizeof(buf_msg), 3);
        }
        else
        {
            uint8_t  buf_msg[] = {prop_SysPower, 1, prop_HoodSpeed, 0};
            Mars_uartmsg_send(cmd_set, uart_get_seq_mid(),  buf_msg, sizeof(buf_msg), 3);
        }
    }
    else if (strcmp(data, "fanDown") == 0)
    {
        LOGI("mars", "播报: 好的");
        xm_open_voice_file("107");

        if (t_mars_template_ctx->status.HoodSpeed > 0)
        {
            uint8_t  buf_msg[] = {prop_HoodSpeed, t_mars_template_ctx->status.HoodSpeed-1};
            Mars_uartmsg_send(cmd_set, uart_get_seq_mid(),  buf_msg, sizeof(buf_msg), 3);
        }
        else
        {
            uint8_t  buf_msg[] = {prop_HoodSpeed, 3};
            Mars_uartmsg_send(cmd_set, uart_get_seq_mid(),  buf_msg, sizeof(buf_msg), 3);
        }
    }
}

void voice_handle_vol_on_off(char* data)
{
    LOGI("mars", "播报: 好的");
    xm_open_voice_file("107");  
    mars_store_voicestatus();
}

void voice_handle_light_on_off(char* data)
{
    mars_template_ctx_t *t_mars_template_ctx = mars_dm_get_ctx();
    if (strcmp(data, "lightTurnOn") == 0)
    {
        LOGI("mars", "播报: 好的");
        xm_open_voice_file("107");
        uint8_t buf_msg[] = {prop_SysPower, 1, prop_HoodLight, 1};
        Mars_uartmsg_send(cmd_set, uart_get_seq_mid(),  buf_msg, sizeof(buf_msg), 3);
    }
    else if (strcmp(data, "lightTurnOff") == 0)
    {
        LOGI("mars", "播报: 好的");
        xm_open_voice_file("107");
        uint8_t buf_msg[] = {prop_HoodLight, 0};
        Mars_uartmsg_send(cmd_set, uart_get_seq_mid(),  buf_msg, sizeof(buf_msg), 3);
    }
}

void voice_handle_stov_on_off(char* data)
{
    mars_template_ctx_t *t_mars_template_ctx = mars_dm_get_ctx();
    if (strcmp(data, "ovenStart") == 0)
    {
        LOGI("mars", "播报: 好的");
        xm_open_voice_file("107");
        uint8_t buf_msg[] = {prop_LStOvOperation, 0};
        Mars_uartmsg_send(cmd_set, uart_get_seq_mid(),  buf_msg, sizeof(buf_msg), 3);
    }
    else if (strcmp(data, "ovenPause") == 0)
    {
        LOGI("mars", "播报: 好的");
        xm_open_voice_file("107");
        uint8_t buf_msg[] = {prop_LStOvOperation, 1};
        Mars_uartmsg_send(cmd_set, uart_get_seq_mid(),  buf_msg, sizeof(buf_msg), 3);
    }
    else if (strcmp(data, "ovenTurnoff") == 0)
    {
        LOGI("mars", "播报: 好的");
        xm_open_voice_file("107");
        uint8_t buf_msg[] = {prop_LStOvOperation, 2};
        Mars_uartmsg_send(cmd_set, uart_get_seq_mid(),  buf_msg, sizeof(buf_msg), 3);
    }
}
typedef struct menu_para
{
    char*       menu;
    uint8_t     mode;
    uint16_t    temp;
    uint16_t    time;

    char*       rep_index;
    char*       rep_string;
}menu_para_t;
menu_para_t menu_para_tab[] = 
{
    {"Menu1",   1, 100, 15, "111", "好的,开始蒸鱼"  }, 
    {"Menu2",   1, 100, 30, "112", "好的,开始蒸肉"  },
    {"Menu3",   1, 100, 15, "113", "好的,开始蒸蛋"  },
    {"Menu4",   4, 120, 40, "114", "好的,开始蒸排骨"},
    {"Menu5",   1, 100, 20, "115", "好的,开始蒸海鲜"},
    {"Menu6",   1, 100, 20, "116", "好的,开始蒸馒头"},
    {"Menu6a",  1, 100, 20, "117", "好的,开始蒸包子"},
    {"Menu6b",  1, 100, 20, "118", "好的,开始蒸花卷"},
    {"Menu7",   1, 100, 30, "119", "好的,开始蒸米饭"},
    {"Menu8",   1, 100, 25, "120", "好的,开始蒸红薯"},
    {"Menu8a",  1, 100, 25, "121", "好的,开始蒸南瓜"},
    {"Menu8b",  1, 100, 25, "122", "好的,开始蒸玉米"},
    {"Menu9",   1, 100, 15, "123", "好的,开始蒸蔬菜"},
    {"Menu10",  1, 100, 60, "124", "好的,开始煲汤"  },
    {"Menu11", 36, 170, 20, "125", "好的,开始烤饼干"},
    {"Menu12", 35, 200, 20, "126", "好的,开始烤蛋挞"},
    {"Menu13", 35, 180, 20, "127", "好的,开始烤鸡翅"},
    {"Menu14", 35, 200, 20, "128", "好的,开始烤串"  },
    {"Menu15", 35, 180, 30, "129", "好的,开始烤鸡"  },
    {"Menu16", 36, 150, 35, "130", "好的,开始烤蛋糕"},
    {"Menu17", 36, 160, 20, "131", "好的,开始烤面包"},
    {"Menu18", 36, 180, 20, "132", "好的,开始烤披萨"},
    {"Menu19", 36, 160, 35, "133", "好的,开始烤酥饼"},
    {"Menu20", 42, 200, 15, "134", "好的,开始炸薯条"},
};
void voice_handle_recipe_on_off(char* data)
{
    mars_template_ctx_t *t_mars_template_ctx = mars_dm_get_ctx();
    if (t_mars_template_ctx->status.LStOvState == 0)
    {
        int cnt = sizeof(menu_para_tab) / sizeof(menu_para_tab[0]);
        for (int inx = 0; inx < cnt; inx++)
        {
            if (strcmp(menu_para_tab[inx].menu, data) == 0)
            {
                LOGI("mars", "播报: %s (%s)", menu_para_tab[inx].rep_string, menu_para_tab[inx].rep_index);
                xm_open_voice_file(menu_para_tab[inx].rep_index);

                uint8_t buf_msg2[] = {
                prop_SysPower,   0X01,
                prop_LMultiMode, 0x00, 
                prop_LStOvMode,     menu_para_tab[inx].mode, 
                prop_LStOvSetTemp,  menu_para_tab[inx].temp/256, menu_para_tab[inx].temp%256, 
                prop_LStOvSetTimer, menu_para_tab[inx].time/256, menu_para_tab[inx].time%256,             
                prop_LStOvOperation, 0x00};
                Mars_uartmsg_send(cmd_set, uart_get_seq_mid(),  buf_msg2, sizeof(buf_msg2), 3);
            }
        }
    }
    else
    {
        LOGI("mars", "播报: 请先关闭蒸烤箱");
        xm_open_voice_file("135");
    }
}

voice_en_cn_map map_table[] = 
{
    {"deviceTurnOff",   "关机|关闭集成灶",       voice_handle_sys_on_off},
    {"deviceTurnOn",    "开机|打开集成灶",       voice_handle_sys_on_off},

    {"fanTurnOn",       "打开烟机",             voice_handle_hood_on_off},
    {"fanTurnOff",      "关闭烟机",             voice_handle_hood_on_off},
    {"fanStepThd",      "风速三档",             voice_handle_hood_on_off},
    {"fanStepSec",      "风速二档",             voice_handle_hood_on_off},
    {"fanStepOne",      "风速一档",             voice_handle_hood_on_off},
    {"fanUp",           "增大风速",             voice_handle_hood_on_off},
    {"fanDown",         "减小风速",             voice_handle_hood_on_off},

    {"VolMax",          "音量最大",             voice_handle_vol_on_off},
    {"VolMin",          "音量最小",             voice_handle_vol_on_off},
    {"VodInc",          "增大音量",             voice_handle_vol_on_off},
    {"VodDec",          "减小音量",             voice_handle_vol_on_off},

    {"lightTurnOn",     "打开照明",             voice_handle_light_on_off},
    {"lightTurnOff",    "关闭照明",             voice_handle_light_on_off},

    {"ovenStart",       "启动蒸烤箱",           voice_handle_stov_on_off},
    {"ovenPause",       "暂停蒸烤箱",           voice_handle_stov_on_off},
    {"ovenTurnoff",     "关闭蒸烤箱",           voice_handle_stov_on_off},

    {"Menu1",           "开始蒸鱼",             voice_handle_recipe_on_off},
    {"Menu2",           "开始蒸肉",             voice_handle_recipe_on_off},
    {"Menu3",           "开始蒸蛋" ,            voice_handle_recipe_on_off},
    {"Menu4",           "开始蒸排骨",           voice_handle_recipe_on_off},
    {"Menu5",           "开始蒸海鲜",           voice_handle_recipe_on_off},
    {"Menu6",           "开始蒸馒头",           voice_handle_recipe_on_off},
    {"Menu6a",          "开始蒸包子",           voice_handle_recipe_on_off},
    {"Menu6b",          "开始蒸花卷",           voice_handle_recipe_on_off},
    {"Menu7",           "开始蒸米饭",           voice_handle_recipe_on_off},
    {"Menu8",           "开始蒸红薯",           voice_handle_recipe_on_off},
    {"Menu8a",          "开始蒸南瓜",           voice_handle_recipe_on_off},
    {"Menu8b",          "开始蒸玉米",           voice_handle_recipe_on_off},
    {"Menu9",           "开始蒸蔬菜",           voice_handle_recipe_on_off},
    {"Menu10",          "开始煲汤",             voice_handle_recipe_on_off},
    {"Menu11",          "开始烤饼干",           voice_handle_recipe_on_off},
    {"Menu12",          "开始烤蛋挞",           voice_handle_recipe_on_off},
    {"Menu13",          "开始烤鸡翅",           voice_handle_recipe_on_off},
    {"Menu14",          "开始烤串"  ,           voice_handle_recipe_on_off},
    {"Menu15",          "开始烤鸡"  ,           voice_handle_recipe_on_off},
    {"Menu16",          "开始烤蛋糕",           voice_handle_recipe_on_off},
    {"Menu17",          "开始烤面包",           voice_handle_recipe_on_off},
    {"Menu18",          "开始烤披萨",           voice_handle_recipe_on_off},
    {"Menu19",          "开始烤酥饼",           voice_handle_recipe_on_off},
    {"Menu20",          "开始炸薯条",           voice_handle_recipe_on_off},
};
static int voice_count = sizeof(map_table) / sizeof(map_table[0]);
static int voice_index = -1;  
static int xm_voice_callback(char* data,int len)
{
    int index = 0;
    for (index=0; index<voice_count; index++)
    {
        if ((strcmp(map_table[index].en, data) == 0) && (map_table[index].cb != NULL))
        {
            LOGW("mars", "收到离线语音(%d): %s %s", index, map_table[index].en, map_table[index].cn);
            voice_index = index;
            //map_table[index].cb(data);
            break;
        }
    }

    if (index >= voice_count)
    {
        LOGE("mars", "error 收到未识别语音: %s", data);

        if (strcmp(data, "wakeup") == 0)
        {
            mars_devmngr_getstatus(NULL, NULL);
        }
    }

    return 0;
}
typedef int (*xm_voice_callback_t)( char * /*data*/,int /*len*/);
extern int xm_voicectrl_register(xm_voice_callback_t xm_callback);

int decimal_bcd_code(int decimal)
{
	int sum = 0;  //sum返回的BCD码
 
	for (int i = 0; decimal > 0; i++)
	{
		sum |= ((decimal % 10 ) << ( 4*i)); 
		decimal /= 10;
	}
 
	return sum;
}

static aos_task_t task_mars_voice_msg_recv;
void mars_voicemsg_recv_thread(void *argv)
{
    while(true)
    {
        if ((voice_index >= 0) && (voice_index < voice_count))
        {
            int index = voice_index;
            voice_index = -1;
            map_table[index].cb(map_table[index].en);
        }

        aos_msleep(50);
    }

    aos_task_exit(0);
}

int mars_devmgr_init(void)
{
    aos_register_event_filter(EV_DEVFUNC, mars_dev_event_callback, NULL);

    xm_voice_set_communication_mode(COMMUNICATION_I2C);
    xm_voicectrl_register(xm_voice_callback);      
    aos_msleep(1000);
    //LOGW("mars", "语音芯片版本: %s", xm_voice_get_voice_version());
    //aos_msleep(1000);
    xm_voice_set_volume(g_volume_gear);
    int ret = aos_task_new_ext(&task_mars_voice_msg_recv, "task_mars_voice_msg_recv", mars_voicemsg_recv_thread, NULL, 2*1024, AOS_DEFAULT_APP_PRI);
    if (ret != 0)
        LOGE("mars", "aos_task_new_ext: failed!!! (串口处理:声音数据接收线程)");


    //转换wifi模组版本号
    // char tmp_version[10] ={0};
    // strncpy(tmp_version, aos_get_app_version(), sizeof(tmp_version)); //like this: "0.1.4"
    // tmp_version[1] = 0;
    // tmp_version[3] = 0;
    // tmp_version[5] = 0;
    // g_user_example_ctx.status.WifiSWVersion[0] = (uint8_t)atoi(tmp_version);
    // g_user_example_ctx.status.WifiSWVersion[1] = (uint8_t)atoi(tmp_version+2);
    // g_user_example_ctx.status.WifiSWVersion[2] = (uint8_t)atoi(tmp_version+4); 

    char tmp_version[10] ={0};
    strncpy(tmp_version, aos_get_app_version(), sizeof(tmp_version)); //like this: "1.4"
    tmp_version[1] = 0;
    tmp_version[3] = 0;
    int ver = decimal_bcd_code((uint8_t)atoi(tmp_version)*10 + (uint8_t)atoi(tmp_version+2));
    g_user_example_ctx.status.WifiSWVersion[0] = ver;
    LOGI("mars", "app ver: %s (%02d 0x%02X)", aos_get_app_version(), g_user_example_ctx.status.WifiSWVersion[0], g_user_example_ctx.status.WifiSWVersion[0]);

    extern void mars_get_ota_para();
    mars_get_ota_para();

    return 0;
}