/*
 * @Description  : 
 * @Author       : zhoubw
 * @Date         : 2022-08-17 16:57:42
 * @LastEditors  : zhoubw
 * @LastEditTime : 2022-11-29 09:21:44
 * @FilePath     : /tg7100c/Products/example/mars_template/mars_devfunc/mars_stove.c
 */

#include <stdio.h>
#include "cJSON.h"
#include "iot_export.h"
#include <hal/wifi.h>

#include "../dev_config.h"
#include "../mars_driver/mars_cloud.h"
#include "mars_stove.h"
#include "mars_httpc.h"

#if MARS_STOVE

#define VALID_BIT(n)    ((uint64_t)1 << (n - prop_LStoveStatus))

#pragma pack(1)
#define TEMP_PACK_SUM       24
#define TEMP_FREQ           40


// typedef struct M_STOVE_TEMP{
//     uint64_t time;
//     int r_temp;
//     // int l_temp;
// }m_stovetemp_st;

// static m_stovetemp_st g_stove_temp[TEMP_PACK_SUM+1] = {0};

#pragma pack()



#define STR_TEMPERINFO "{\"time\":\"%ld\",\"temp\":\"%d\"},"
#define STR_TEMPERPOST  "{\"deviceMac\":\"%s\",\"iotId\":\"%s\",\"token\":\"%s\",\"curveKey\":\"%d\",\"cookCurveTempDTOS\":[%s]}"
#define STR_TEMPERPOST_HEAD  "{\"deviceMac\":\"%s\",\"iotId\":\"%s\",\"token\":\"%s\",\"curveKey\":\"%d\",\"cookCurveTempDTOS\":["
#define STR_TEMPERPOST_TAIL "]}"

static char g_stove_tempstr[2048] = {0};
static uint8_t g_stove_temp_cnt = 0;
// #define STR_TEMPERPOST  "{\"deviceMac\":\"%s\",\"iotId\":\"%s\",\"Temp\":\"%d\",\"curveKey\":\"%d\",\"creteTime\":\"%d\"}"
// #define URL_TEMPERPOST "http://mcook.dev.marssenger.net/menu-anon/addCookCurve"
// #define URL_TEMPERPOST "192.168.1.108:12300"

void mars_stove_uartMsgFromSlave(uartmsg_que_t *msg, 
                                mars_template_ctx_t *mars_template_ctx, 
                                uint16_t *index, bool *report_en, uint8_t *nak)
{
    switch (msg->msg_buf[(*index)])
    {
        case prop_LStoveStatus:
        {
            //get/get-ack/event
            if (cmd_getack == msg->cmd || cmd_event == msg->cmd){
                mars_template_ctx->status.LStoveStatus = msg->msg_buf[(*index)+1];
                mars_template_ctx->stove_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);

                if (mars_template_ctx->status.LStoveStatus && mars_template_ctx->status.RStoveStatus){      //如果右灶已打开的情况下打开左灶
                    mars_template_ctx->status.fsydHoodMinSpeed = 2;
                }

                *report_en = true;
            }else{
                (*nak) = NAK_ERROR_CMDCODE_NOSUPPORT;
            }
            (*index)+=1;
            break;
        }
        case prop_RStoveStatus:
        {
            //get/get-ack/event
            if (cmd_getack == msg->cmd || cmd_event == msg->cmd){
                LOGI("mars", "上报属性: 当前右灶状态 = %d", msg->msg_buf[(*index)+1]);
                if (msg->msg_buf[(*index)+1] == 0x01)
                {
                    if (mars_template_ctx->status.RStoveStatus == 0)
                    {
                        mars_template_ctx->status.CurKeyvalue = rand();
                        LOGI("mars", "右灶状态改变: 右灶打开 (生成随机数=%d)", mars_template_ctx->status.CurKeyvalue);
                    }
                }

                mars_template_ctx->status.RStoveStatus = msg->msg_buf[(*index)+1];
                mars_template_ctx->stove_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
                *report_en = true;
            }else{
                *nak = NAK_ERROR_CMDCODE_NOSUPPORT;
            }
            (*index)+=1;
            break;
        }
        case prop_HoodStoveLink:
        {
            //get/get-ack/event
            if (cmd_getack == msg->cmd || cmd_event == msg->cmd){
                mars_template_ctx->status.HoodStoveLink = msg->msg_buf[(*index)+1];
                mars_template_ctx->stove_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
                *report_en = true;
            }else{
                *nak = NAK_ERROR_CMDCODE_NOSUPPORT;
            }
            (*index)+=1;
            break;
        }
        case prop_HoodLightLink:
        {
            //get/get-ack/event
            if (cmd_getack == msg->cmd || cmd_event == msg->cmd){
                mars_template_ctx->status.HoodLightLink = msg->msg_buf[(*index)+1];
                mars_template_ctx->stove_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
                *report_en = true;
            }else{
                *nak = NAK_ERROR_CMDCODE_NOSUPPORT;
            }
            (*index)+=1;
            break;
        }
        case prop_RStoveTimingState:
            if (0x03 != mars_template_ctx->status.RStoveTimingState && 0x03 == msg->msg_buf[(*index)+1])
            {
                user_post_event_json(EVENT_RIGHT_STOVE_TIMER_FINISH);
                LOGI("mars", "推送事件: 右灶定时完成推送 (%d -> %d) (0-停止 1-运行 2-暂停 3-完成)", mars_template_ctx->status.RStoveTimingState, msg->msg_buf[(*index)+1]);
            }

            mars_template_ctx->status.RStoveTimingState = msg->msg_buf[(*index)+1];
            mars_template_ctx->stove_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            *report_en = true;
            (*index)+=1;
            break;

        case prop_LStoveTimingState:
        {
            if (0x03 != mars_template_ctx->status.LStoveTimingState && 0x03 == msg->msg_buf[(*index)+1])
            {
                user_post_event_json(EVENT_LEFT_STOVE_TIMER_FINISH);
                LOGI("mars", "推送事件: 左灶定时完成推送 (%d -> %d) (0-停止 1-运行 2-暂停 3-完成)", mars_template_ctx->status.LStoveTimingState, msg->msg_buf[(*index)+1]);
            }

            mars_template_ctx->status.LStoveTimingState = msg->msg_buf[(*index)+1];
            mars_template_ctx->stove_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            *report_en = true;
            (*index)+=1;
            break;
        }
        case prop_RStoveTimingSet:
        {
            mars_template_ctx->status.RStoveTimingSet = msg->msg_buf[(*index)+1];
            mars_template_ctx->stove_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            *report_en = true;
            (*index)+=1;
            break;
        }

        case prop_LStoveTimingSet:
        {
            mars_template_ctx->status.LStoveTimingSet = msg->msg_buf[(*index)+1];
            mars_template_ctx->stove_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            *report_en = true;
            (*index)+=1;
            break;
        }

        case prop_RStoveTimingLeft:
        {
            mars_template_ctx->status.RStoveTimingLeft = msg->msg_buf[(*index)+1];
            mars_template_ctx->stove_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            *report_en = true;
            (*index)+=1;
            break;
        }

        case prop_LStoveTimingLeft:
        {
            mars_template_ctx->status.LStoveTimingLeft = msg->msg_buf[(*index)+1];
            mars_template_ctx->stove_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            *report_en = true;
            (*index)+=1;
            break;
        }

        case prop_RStoveSwitch:
        {
            //get-ack/set/event
            if (cmd_getack == msg->cmd || cmd_event == msg->cmd){
                LOGI("mars", "右灶通断阀: = %d", msg->msg_buf[(*index)+1]);
                mars_template_ctx->status.RStoveSwitch = msg->msg_buf[(*index)+1];
                mars_template_ctx->stove_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
                *report_en = true;
            }else{
                *nak = NAK_ERROR_CMDCODE_NOSUPPORT;
            }
            (*index)+=1;
            break;
        }
        case prop_HoodSpeed:
        {
            //get/get-ack/set/event
            if (cmd_getack == msg->cmd || cmd_event == msg->cmd){
                mars_template_ctx->status.HoodSpeed = msg->msg_buf[(*index)+1];
                LOGI("mars", "收到属性: 当前烟机档位 = %d", mars_template_ctx->status.HoodSpeed);
                mars_template_ctx->stove_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
                *report_en = true;
            }else{
                *nak = NAK_ERROR_CMDCODE_NOSUPPORT;
            }
            (*index)+=1;
            break;
        }
        case prop_HoodLight:
        {
            //get/get-ack/set/event
            if (cmd_getack == msg->cmd || cmd_event == msg->cmd){
                mars_template_ctx->status.HoodLight = msg->msg_buf[(*index)+1];
                mars_template_ctx->stove_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
                *report_en = true;
            }else{
                *nak = NAK_ERROR_CMDCODE_NOSUPPORT;
            }
            (*index)+=1;
            break;
        }
        case prop_HoodOffLeftTime:
        {
            //get/get-ack/event
            if (cmd_getack == msg->cmd || cmd_event == msg->cmd){
                mars_template_ctx->status.HoodOffLeftTime = msg->msg_buf[(*index)+1];
                mars_template_ctx->stove_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
                *report_en = true;
            }else{
                *nak = NAK_ERROR_CMDCODE_NOSUPPORT;
            }
            (*index)+=1;
            break;
        }
        case prop_HoodOffTimer:
        {
            //get/get-ack/set/event
            if (cmd_getack == msg->cmd || cmd_event == msg->cmd){
                mars_template_ctx->status.HoodOffTimer = msg->msg_buf[(*index)+1];
                mars_template_ctx->stove_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
                *report_en = true;
            }else{
                *nak = NAK_ERROR_CMDCODE_NOSUPPORT;
            }
            (*index)+=1;
            break;
        }
        case prop_HoodTurnOffRemind:
        {
            //event
            if (cmd_getack == msg->cmd || cmd_event == msg->cmd){
                mars_template_ctx->status.HoodTurnOffRemind = msg->msg_buf[(*index)+1];
                mars_template_ctx->stove_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
                *report_en = true;
            }else{
                *nak = NAK_ERROR_CMDCODE_NOSUPPORT;
            }
            (*index)+=1;
            break;
        } 
        case prop_OilBoxState:
        {
            if (0x01 != mars_template_ctx->status.OilBoxState && 0x01 == msg->msg_buf[(*index)+1])
            { 
                LOGI("mars", "推送事件: 油盒提醒推送OilBoxPush! (%d -> %d) (0-未满 1-已满)", mars_template_ctx->status.OilBoxState, msg->msg_buf[(*index)+1]);
                user_post_event_json(EVENT_LEFT_STOVE_OILBOX_REMIND);
            }
            mars_template_ctx->status.OilBoxState = msg->msg_buf[(*index)+1];
            mars_template_ctx->stove_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            *report_en = true;

            (*index)+=1;
            break;
        }
        case prop_BalconyHeat:
        {
            mars_template_ctx->status.BalconyHeatState = msg->msg_buf[(*index)+1];
            mars_template_ctx->stove_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            *report_en = true;

            (*index)+=1;
            break;
        }
        default:
            *nak = NAK_ERROR_UNKOWN_PROCODE;
            break;
    }
}

void mars_stove_setToSlave(cJSON *root, cJSON *item, mars_template_ctx_t *mars_template_ctx, uint8_t *buf_setmsg, uint16_t *buf_len)
{
    if (NULL == root || NULL == mars_template_ctx || NULL == buf_setmsg || NULL == buf_len)
    {
        return false;
    }

    if ((item = cJSON_GetObjectItem(root, "HoodStoveLink")) != NULL && cJSON_IsNumber(item)) {
        //mars_template_ctx->status.HoodStoveLink = item->valueint;
        buf_setmsg[(*buf_len)++] = prop_HoodStoveLink;//烟灶联动
        buf_setmsg[(*buf_len)++] = item->valueint;
    }
    
    if ((item = cJSON_GetObjectItem(root, "HoodLightLink")) != NULL && cJSON_IsNumber(item)) {
        //mars_template_ctx->status.HoodLightLink = item->valueint;
        buf_setmsg[(*buf_len)++] = prop_HoodLightLink;//烟机照明联动
        buf_setmsg[(*buf_len)++] = item->valueint;
    }

    if ((item = cJSON_GetObjectItem(root, "RStoveTimingOpera")) != NULL && cJSON_IsNumber(item)) {
        //mars_template_ctx->status.RStoveTimingOpera = item->valueint;
        buf_setmsg[(*buf_len)++] = prop_RStoveTimingOpera;//右灶定时动作
        buf_setmsg[(*buf_len)++] = item->valueint;
    }

    if ((item = cJSON_GetObjectItem(root, "RStoveTimingSet")) != NULL && cJSON_IsNumber(item)) {
        //mars_template_ctx->status.RStoveTimingSet = item->valueint;
        buf_setmsg[(*buf_len)++] = prop_RStoveTimingSet;//右灶定时时间
        buf_setmsg[(*buf_len)++] = item->valueint;
    }

    if ((item = cJSON_GetObjectItem(root, "LStoveTimingOpera")) != NULL && cJSON_IsNumber(item)) {
        //mars_template_ctx->status.RStoveTimingOpera = item->valueint;
        buf_setmsg[(*buf_len)++] = prop_LStoveTimingOpera;
        buf_setmsg[(*buf_len)++] = item->valueint;
    }

    if ((item = cJSON_GetObjectItem(root, "LStoveTimingSet")) != NULL && cJSON_IsNumber(item)) {
        //mars_template_ctx->status.RStoveTimingSet = item->valueint;
        buf_setmsg[(*buf_len)++] = prop_LStoveTimingSet;
        buf_setmsg[(*buf_len)++] = item->valueint;
    }

    if ((item = cJSON_GetObjectItem(root, "HoodFireTurnOff")) != NULL && cJSON_IsNumber(item)) {
        //mars_template_ctx->status.HoodFireTurnOff = item->valueint;
        buf_setmsg[(*buf_len)++] = prop_HoodFireTurnOff;  //灶具关火
        buf_setmsg[(*buf_len)++] = item->valueint;
    }

    if ((item = cJSON_GetObjectItem(root, "RStoveSwitch")) != NULL && cJSON_IsNumber(item)) {
        //mars_template_ctx->status.RStoveSwitch = item->valueint;
        buf_setmsg[(*buf_len)++] = prop_RStoveSwitch;   //右灶通断阀
        buf_setmsg[(*buf_len)++] = item->valueint;
    }

    if ((item = cJSON_GetObjectItem(root, "HoodSpeed")) != NULL && cJSON_IsNumber(item)) {
        //mars_template_ctx->status.HoodSpeed = item->valueint;
        buf_setmsg[(*buf_len)++] = prop_HoodSpeed;//烟机风速
        buf_setmsg[(*buf_len)++] = item->valueint;
    }

    if ((item = cJSON_GetObjectItem(root, "HoodLight")) != NULL && cJSON_IsNumber(item)) {
        //mars_template_ctx->status.HoodLight = item->valueint;
        buf_setmsg[(*buf_len)++] = prop_HoodLight;//烟机照明
        buf_setmsg[(*buf_len)++] = item->valueint;
    }

    if ((item = cJSON_GetObjectItem(root, "HoodOffTimer")) != NULL && cJSON_IsNumber(item)) {
        //mars_template_ctx->status.HoodOffTimer = item->valueint;
        buf_setmsg[(*buf_len)++] = prop_HoodOffTimer;
        buf_setmsg[(*buf_len)++] = item->valueint;
    }

    if ((item = cJSON_GetObjectItem(root, "OilBoxState")) != NULL && cJSON_IsNumber(item)) {
        //mars_template_ctx->status.OilBoxState = item->valueint;
        buf_setmsg[(*buf_len)++] = prop_OilBoxState;
        buf_setmsg[(*buf_len)++] = item->valueint;
    }

    if ((item = cJSON_GetObjectItem(root, "BalconyHeatSwitch")) != NULL && cJSON_IsNumber(item)) {
        buf_setmsg[(*buf_len)++] = prop_BalconyHeat;
        buf_setmsg[(*buf_len)++] = item->valueint;
    }
}



void mars_stove_changeReport(cJSON *proot, mars_template_ctx_t *mars_template_ctx)
{
    for (uint8_t index=prop_LStoveStatus; index<=prop_BalconyHeat; ++index)
    {
        if (mars_template_ctx->stove_reportflg & VALID_BIT(index))
        {
            switch (index)
            {
                case prop_LStoveStatus:
                {
                    //LOGI("mars", "mars_stove_changeReport: LStoveStatus");
                    cJSON_AddNumberToObject(proot, "LStoveStatus", mars_template_ctx->status.LStoveStatus);
                    break;
                }
                case prop_RStoveStatus:
                {
                    //LOGI("mars", "mars_stove_changeReport: RStoveStatus");
                    cJSON_AddNumberToObject(proot, "RStoveStatus", mars_template_ctx->status.RStoveStatus);
                    break;
                }
                case prop_HoodStoveLink:
                {
                    //LOGI("mars", "mars_stove_changeReport: HoodStoveLink");
                    cJSON_AddNumberToObject(proot, "HoodStoveLink", mars_template_ctx->status.HoodStoveLink);
                    break;
                }
                case prop_HoodLightLink:
                {
                    //LOGI("mars", "mars_stove_changeReport: HoodLightLink");
                    cJSON_AddNumberToObject(proot, "HoodLightLink", mars_template_ctx->status.HoodLightLink);
                    break;
                }
                case prop_RStoveTimingState:
                {
                    //LOGI("mars", "mars_stove_changeReport: RStoveTimingState");
                    cJSON_AddNumberToObject(proot, "RStoveTimingState", mars_template_ctx->status.RStoveTimingState);
                    break;
                }
                case prop_LStoveTimingState:
                {
                    //LOGI("mars", "mars_stove_changeReport: RStoveTimingState");
                    cJSON_AddNumberToObject(proot, "LStoveTimingState", mars_template_ctx->status.LStoveTimingState);
                    break;
                }
                // case prop_RStoveTimingOpera:
                // {
                //     cJSON_AddNumberToObject(proot, "RStoveTimingOpera", mars_template_ctx->status.RStoveTimingOpera);
                //     break;
                // }
                case prop_RStoveTimingSet:
                {
                    //LOGI("mars", "mars_stove_changeReport: RStoveTimingSet");
                    cJSON_AddNumberToObject(proot, "RStoveTimingSet", mars_template_ctx->status.RStoveTimingSet);
                    break;
                }
                case prop_LStoveTimingSet:
                {
                    //LOGI("mars", "mars_stove_changeReport: RStoveTimingSet");
                    cJSON_AddNumberToObject(proot, "LStoveTimingSet", mars_template_ctx->status.LStoveTimingSet);
                    break;
                }
                case prop_RStoveTimingLeft:
                {
                    //LOGI("mars", "mars_stove_changeReport: RStoveTimingLeft");
                    cJSON_AddNumberToObject(proot, "RStoveTimingLeft", mars_template_ctx->status.RStoveTimingLeft);
                    break;
                }
                case prop_LStoveTimingLeft:
                {
                    //LOGI("mars", "mars_stove_changeReport: RStoveTimingLeft");
                    cJSON_AddNumberToObject(proot, "LStoveTimingLeft", mars_template_ctx->status.LStoveTimingLeft);
                    break;
                }
                case prop_HoodFireTurnOff:
                {
                    //LOGI("mars", "mars_stove_changeReport: HoodFireTurnOff");
                    cJSON_AddNumberToObject(proot, "HoodFireTurnOff", mars_template_ctx->status.HoodFireTurnOff);
                    break;
                }
                case prop_LThermocoupleState:
                {
                    //LOGI("mars", "mars_stove_changeReport: LThermocoupleState");
                    cJSON_AddNumberToObject(proot, "LThermocoupleState", mars_template_ctx->status.LThermocoupleState);
                    break;
                }
                case prop_RThermocoupleState:
                {
                    //LOGI("mars", "mars_stove_changeReport: RThermocoupleState");
                    cJSON_AddNumberToObject(proot, "RThermocoupleState", mars_template_ctx->status.RThermocoupleState);
                    break;
                }
                case prop_RStoveSwitch:
                {
                    //LOGI("mars", "mars_stove_changeReport: RStoveSwitch");
                    cJSON_AddNumberToObject(proot, "RStoveSwitch", mars_template_ctx->status.RStoveSwitch);
                    break;
                }
                // case prop_RAuxiliarySwitch:
                // {
                //     cJSON_AddNumberToObject(proot, "RAuxiliarySwitch", mars_template_ctx->status.RAuxiliarySwitch);
                //     break;
                // }
                // case prop_RAuxiliaryTemp:
                // {
                //     cJSON_AddNumberToObject(proot, "RAuxiliaryTemp", mars_template_ctx->status.RAuxiliaryTemp);
                //     break;
                // }
                // case prop_ROilTemp:                  //使用http上报
                // {
                //     cJSON_AddNumberToObject(proot, "ROilTemp", mars_template_ctx->status.ROilTemp);
                //     break;
                // }
                case prop_HoodSpeed:
                {
                    //LOGI("mars", "mars_stove_changeReport: HoodSpeed");
                    cJSON_AddNumberToObject(proot, "HoodSpeed", mars_template_ctx->status.HoodSpeed);
                    break;
                }
                case prop_HoodLight:
                {
                    //LOGI("mars", "mars_stove_changeReport: HoodLight");
                    cJSON_AddNumberToObject(proot, "HoodLight", mars_template_ctx->status.HoodLight);
                    break;
                }
                case prop_HoodOffLeftTime:
                {
                    //LOGI("mars", "mars_stove_changeReport: HoodOffLeftTime");
                    cJSON_AddNumberToObject(proot, "HoodOffLeftTime", mars_template_ctx->status.HoodOffLeftTime);
                    break;
                }
                // case prop_FsydSwitch:
                // {
                //     cJSON_AddNumberToObject(proot, "SmartSmokeSwitch", mars_template_ctx->status.FsydSwitch);
                //     break;
                // }
                // case prop_HoodSteplessSpeed:
                // {
                //     cJSON_AddNumberToObject(proot, "HoodSteplessSpeed", mars_template_ctx->status.HoodSteplessSpeed);
                //     break;
                // }
                case prop_HoodOffTimer:
                {
                    //LOGI("mars", "mars_stove_changeReport: HoodOffTimer");
                    cJSON_AddNumberToObject(proot, "HoodOffTimer", mars_template_ctx->status.HoodOffTimer);
                    break;
                }
                case prop_HoodTurnOffRemind:
                {
                    //LOGI("mars", "mars_stove_changeReport: HoodTurnOffRemind");
                    cJSON_AddNumberToObject(proot, "HoodTurnOffRemind", mars_template_ctx->status.HoodTurnOffRemind);
                    break;
                }
                case prop_OilBoxState:
                {
                    //LOGI("mars", "mars_stove_changeReport: OilBoxState");
                    cJSON_AddNumberToObject(proot, "OilBoxState", mars_template_ctx->status.OilBoxState);
                    break;
                }
                case prop_BalconyHeat:
                {
                    cJSON_AddNumberToObject(proot, "BalconyHeatSwitch", mars_template_ctx->status.BalconyHeatState);
                    break;
                }
                // case prop_RMovePotLowHeatSwitch:
                // {
                //     cJSON_AddNumberToObject(proot, "RMovePotLowHeatSwitch", mars_template_ctx->status.RMovePotLowHeatSwitch);
                //     break;
                // }
                // case prop_OilTempSwitch:
                // {
                //     cJSON_AddNumberToObject(proot, "OilTempSwitch", mars_template_ctx->status.OilTempSwitch);
                // }
                default:
                {
                    //LOGI("mars", "mars_stove_changeReport: default");
                }
            }
        }
    }

    mars_template_ctx->stove_reportflg = 0;
}
#endif
