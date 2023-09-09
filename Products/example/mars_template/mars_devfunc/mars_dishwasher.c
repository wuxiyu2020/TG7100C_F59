/*
 * @Description  : 
 * @Author       : zhoubw
 * @Date         : 2022-08-17 17:00:30
 * @LastEditors  : zhoubw
 * @LastEditTime : 2022-11-17 15:00:47
 * @FilePath     : /tg7100c/Products/example/mars_template/mars_devfunc/mars_dishwasher.c
 */
#include "mars_dishwasher.h"

#if MARS_DISHWASHER

#define         VALID_BIT(n)            ((uint64_t)1 << (n - prop_CmdWashAction))

void mars_dishwasher_uartMsgFromSlave(uartmsg_que_t *msg, 
                                mars_template_ctx_t *mars_template_ctx, 
                                uint16_t *index, bool *report_en, uint8_t *nak)
{
    switch (msg->msg_buf[(*index)])
    {
        // case prop_CmdWashAction:
        // {
        //     //washaction    
        //     //set
        //     if (msg->msg_buf[(*index)+1] >= 0 && msg->msg_buf[(*index)+1] <= 4){
        //         mars_template_ctx->status.CmdWashAction = msg->msg_buf[(*index)+1];
                // *report_en = true;
        //     }
        //     (*index)+=1;
        //     break;
        // }
        case prop_DataRunState:
        {
            //workstatus
            //get-ack/event
            if (cmd_get == msg->cmd || cmd_event == msg->cmd){
                if(msg->msg_buf[(*index)+1] >= 0 && msg->msg_buf[(*index)+1] <= 5){
                    mars_template_ctx->status.DataRunState = msg->msg_buf[(*index)+1];
                    mars_template_ctx->dishwasher_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
                    *report_en = true;
                }else{
                    *nak = NAK_ERROR_PROVALUE;
                }
            }else{
                *nak = NAK_ERROR_CMDCODE_NOSUPPORT;
            }
            (*index)+=1;
            break;
        }
        case prop_ParaWashMode:
        {
            //get-ack/mode
            if (cmd_get == msg->cmd || cmd_event == msg->cmd){
                if(msg->msg_buf[(*index)+1] >= 0 && msg->msg_buf[(*index)+1] <= 7){
                    mars_template_ctx->status.ParaWashMode =msg->msg_buf[(*index)+1];
                    mars_template_ctx->dishwasher_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
                    *report_en = true;
                }else{
                    *nak = NAK_ERROR_PROVALUE;
                }
            }else{
                *nak = NAK_ERROR_CMDCODE_NOSUPPORT;
            }
            (*index)+=1;
            break;
        }
        case prop_ParaAttachMode:
        {
            //get-ack/set/event
            if (cmd_get == msg->cmd || cmd_event == msg->cmd){
                mars_template_ctx->status.ParaAttachMode = msg->msg_buf[(*index)+1];
                mars_template_ctx->dishwasher_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
                *report_en = true;
            }else{
                *nak = NAK_ERROR_CMDCODE_NOSUPPORT;
            }
            (*index)+=1;
            break;
        }
        case prop_ParaDryTime:
        {
            //get-ack/set/event
            if (cmd_get == msg->cmd || cmd_event == msg->cmd){
                mars_template_ctx->status.ParaDryTime = msg->msg_buf[(*index)+1];
                mars_template_ctx->dishwasher_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
                *report_en = true;
            }else{
                *nak = NAK_ERROR_CMDCODE_NOSUPPORT;
            }
            (*index)+=1;
            break;
        }
        case prop_ParaHalfWash:
        {
            //get-ack/set/event
            if (cmd_get == msg->cmd || cmd_event == msg->cmd){
                if(msg->msg_buf[(*index)+1] >= 0 && msg->msg_buf[(*index)+1] <= 2){
                    mars_template_ctx->status.ParaHalfWash = msg->msg_buf[(*index)+1];
                    mars_template_ctx->dishwasher_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
                    *report_en = true;
                }else{
                    *nak = NAK_ERROR_PROVALUE;
                }
            }else{
                *nak = NAK_ERROR_CMDCODE_NOSUPPORT;
            }
            (*index)+=1;
            break;
        }
        case prop_ParaAppointTime:
        {
            //get-ack/set/event
            if (cmd_get == msg->cmd || cmd_event == msg->cmd){
                uint16_t u16_timevalue = (uint16_t)(msg->msg_buf[(*index)+1] << 8) | (uint16_t)msg->msg_buf[(*index)+2];
                if(u16_timevalue <= 1440){
                    mars_template_ctx->status.ParaAppointTime = u16_timevalue;
                    mars_template_ctx->dishwasher_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
                    *report_en = true;
                }else{
                    *nak = NAK_ERROR_PROVALUE;
                }
            }else{
                *nak = NAK_ERROR_CMDCODE_NOSUPPORT;
            }
            (*index)+=2;
            break;
        }
        case prop_DataWashStep:
        {
            //get-ack/event
            if (cmd_get == msg->cmd || cmd_event == msg->cmd){
                if(msg->msg_buf[(*index)+1] >= 0 && msg->msg_buf[(*index)+1] <= 0x07){
                    mars_template_ctx->status.DataWashStep = msg->msg_buf[(*index)+1];
                    mars_template_ctx->dishwasher_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
                    *report_en = true;
                }else{
                    *nak = NAK_ERROR_PROVALUE;
                }
            }else{
                *nak = NAK_ERROR_CMDCODE_NOSUPPORT;
            }
            (*index)+=1;
            break;
        }
        case prop_DataDoorState:
        {
            //get-ack/event
            if (cmd_get == msg->cmd || cmd_event == msg->cmd){
                if(msg->msg_buf[(*index)+1] == 0 || msg->msg_buf[(*index)+1] == 1){
                    mars_template_ctx->status.DataDoorState = msg->msg_buf[(*index)+1];
                    mars_template_ctx->dishwasher_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
                    *report_en = true;
                }else{
                    *nak = NAK_ERROR_PROVALUE;
                }
            }else{
                *nak = NAK_ERROR_CMDCODE_NOSUPPORT;
            }
            (*index)+=1;
            break;
        }
        case prop_DataLeftAppointmentTime:
        {
            //get-ack/event
            if (cmd_get == msg->cmd || cmd_event == msg->cmd){
                uint16_t u16_timevalue = (uint16_t)(msg->msg_buf[(*index)+1] << 8) | (uint16_t)msg->msg_buf[(*index)+2];
                if(u16_timevalue <= 1440){
                    mars_template_ctx->status.DataLeftAppointmentTime = u16_timevalue;
                    mars_template_ctx->dishwasher_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
                    *report_en = true;
                }else{
                    *nak = NAK_ERROR_PROVALUE;
                }
            }else{
                *nak = NAK_ERROR_CMDCODE_NOSUPPORT;
            }
            (*index)+=2;
            break;
        }
        case prop_DataLeftRunTime:
        {
            //get-ack/event
            if (cmd_get == msg->cmd || cmd_event == msg->cmd){
                uint16_t u16_timevalue = (uint16_t)(msg->msg_buf[(*index)+1] << 8) | (uint16_t)msg->msg_buf[(*index)+2];
                if(u16_timevalue <= 1440){
                    mars_template_ctx->status.DataLeftRunTime = u16_timevalue;
                    mars_template_ctx->dishwasher_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
                    *report_en = true;
                }else{
                    *nak = NAK_ERROR_PROVALUE;
                }
            }else{
                *nak = NAK_ERROR_CMDCODE_NOSUPPORT;
            }
            (*index)+=2;
            break;
        }
        case prop_ParaOnlySaltState:
        {
            //get-ack/event
            if (cmd_get == msg->cmd || cmd_event == msg->cmd){
                if(msg->msg_buf[(*index)+1] == 0 || msg->msg_buf[(*index)+1] == 1){
                    mars_template_ctx->status.ParaOnlySaltState = msg->msg_buf[(*index)+1];
                    mars_template_ctx->dishwasher_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
                    report_en = true;
                }else{
                    *nak = NAK_ERROR_PROVALUE;
                }
            }else{
                *nak = NAK_ERROR_CMDCODE_NOSUPPORT;
            }
            (*index)+=1;
            break;
        }
        case prop_ParaSaltGear:
        {
            //get-ack/set/event
            if (cmd_get == msg->cmd || cmd_event == msg->cmd){
                if(msg->msg_buf[(*index)+1] >= 1 || msg->msg_buf[(*index)+1] <= 6){
                    mars_template_ctx->status.ParaSaltGear = msg->msg_buf[(*index)+1];
                    mars_template_ctx->dishwasher_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
                    *report_en = true;
                }else{
                    *nak = NAK_ERROR_PROVALUE;
                }
            }else{
                *nak = NAK_ERROR_CMDCODE_NOSUPPORT;
            }
            (*index)+=1;
            break;
        }
        case prop_ParaOnlyAgentState:
        {
            //get-ack/event
            if (cmd_get == msg->cmd || cmd_event == msg->cmd){
                if(msg->msg_buf[(*index)+1] == 0 || msg->msg_buf[(*index)+1] == 1){
                    mars_template_ctx->status.ParaOnlyAgentState = msg->msg_buf[(*index)+1];
                    mars_template_ctx->dishwasher_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
                    *report_en = true;
                }else{
                    *nak = NAK_ERROR_PROVALUE;
                }                        
            }else{
                *nak = NAK_ERROR_CMDCODE_NOSUPPORT;
            }
            (*index)+=1;
            break;
        }
        case prop_ParaAgentGear:
        {
            //get-ack/set/event
            if (cmd_get == msg->cmd || cmd_event == msg->cmd){
                if(msg->msg_buf[(*index)+1] >= 1 || msg->msg_buf[(*index)+1] <= 3){
                    mars_template_ctx->status.ParaAgentGear = msg->msg_buf[(*index)+1];
                    mars_template_ctx->dishwasher_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
                    *report_en = true;
                }else{
                    *nak = NAK_ERROR_PROVALUE;
                }
            }else{
                *nak = NAK_ERROR_CMDCODE_NOSUPPORT;
            }
            (*index)+=1;
            break;
        }

        default:
            *nak = NAK_ERROR_UNKOWN_PROCODE;
            break;
    }
}


void mars_dishwasher_setToSlave(cJSON *root, cJSON *item, \
                            mars_template_ctx_t *mars_template_ctx, \
                            uint8_t *buf_setmsg, uint16_t *buf_len)
{
    if (NULL == root || NULL == mars_template_ctx || NULL == buf_setmsg || NULL == buf_len){
        return;
    }

    if ((item = cJSON_GetObjectItem(root, "CmdWashAction")) != NULL && cJSON_IsNumber(item)) {
        mars_template_ctx->status.CmdWashAction = item->valueint;
        buf_setmsg[(*buf_len)++] = prop_CmdWashAction;
        buf_setmsg[(*buf_len)++] = mars_template_ctx->status.CmdWashAction;
    }
    if ((item = cJSON_GetObjectItem(root, "ParaWashMode")) != NULL && cJSON_IsNumber(item)) {
        mars_template_ctx->status.ParaWashMode = item->valueint;
        buf_setmsg[(*buf_len)++] = prop_ParaWashMode;
        buf_setmsg[(*buf_len)++] = mars_template_ctx->status.ParaWashMode;
    }
    if ((item = cJSON_GetObjectItem(root, "ParaAttachMode")) != NULL && cJSON_IsNumber(item)) {
        mars_template_ctx->status.ParaAttachMode = item->valueint;
        buf_setmsg[(*buf_len)++] = prop_ParaAttachMode;
        buf_setmsg[(*buf_len)++] = mars_template_ctx->status.ParaAttachMode;
    }
    if ((item = cJSON_GetObjectItem(root, "ParaDryTime")) != NULL && cJSON_IsNumber(item)) {
        mars_template_ctx->status.ParaDryTime = item->valueint;
        buf_setmsg[(*buf_len)++] = prop_ParaDryTime;
        buf_setmsg[(*buf_len)++] = mars_template_ctx->status.ParaDryTime;
    }
    if ((item = cJSON_GetObjectItem(root, "ParaHalfWash")) != NULL && cJSON_IsNumber(item)) {
        mars_template_ctx->status.ParaHalfWash = item->valueint;
        buf_setmsg[(*buf_len)++] = prop_ParaHalfWash;
        buf_setmsg[(*buf_len)++] = mars_template_ctx->status.ParaHalfWash;
    }
    if ((item = cJSON_GetObjectItem(root, "ParaAppointTime")) != NULL && cJSON_IsNumber(item)) {
        mars_template_ctx->status.ParaAppointTime = item->valueint;
        buf_setmsg[(*buf_len)++] = prop_ParaAppointTime;
        buf_setmsg[(*buf_len)++] = mars_template_ctx->status.ParaAppointTime;
    }
    if ((item = cJSON_GetObjectItem(root, "ParaSaltGear")) != NULL && cJSON_IsNumber(item)) {
        mars_template_ctx->status.ParaSaltGear = item->valueint;
        buf_setmsg[(*buf_len)++] = prop_ParaSaltGear;
        buf_setmsg[(*buf_len)++] = mars_template_ctx->status.ParaSaltGear;
    }
    if ((item = cJSON_GetObjectItem(root, "ParaAgentGear")) != NULL && cJSON_IsNumber(item)) {
        mars_template_ctx->status.ParaAgentGear = item->valueint;
        buf_setmsg[(*buf_len)++] = prop_ParaAgentGear;
        buf_setmsg[(*buf_len)++] = mars_template_ctx->status.ParaAgentGear;
    }
}

void mars_dishwasher_changeReport(cJSON *proot, mars_template_ctx_t *mars_template_ctx)
{
    for (uint8_t index=prop_CmdWashAction; index<=prop_ParaAgentGear; ++index){
        if (mars_template_ctx->dishwasher_reportflg & VALID_BIT(index)){
            switch (index){
                case prop_CmdWashAction:
                {
                    cJSON_AddNumberToObject(proot, "CmdWashAction", mars_template_ctx->status.CmdWashAction);
                    break;
                }
                case prop_ParaWashMode:
                {
                    cJSON_AddNumberToObject(proot, "ParaWashMode", mars_template_ctx->status.ParaWashMode);
                    break;
                }
                case prop_ParaAttachMode:
                {
                    cJSON_AddNumberToObject(proot, "ParaAttachMode", mars_template_ctx->status.ParaAttachMode);
                    break;
                }
                case prop_ParaDryTime:
                {
                    cJSON_AddNumberToObject(proot, "ParadryTime", mars_template_ctx->status.ParaDryTime);
                    break;
                }
                case prop_ParaHalfWash:
                {
                    cJSON_AddNumberToObject(proot, "ParaHalfWash", mars_template_ctx->status.ParaHalfWash);
                    break;
                }
                case prop_ParaAppointTime:
                {
                    cJSON_AddNumberToObject(proot, "ParaAppointTime", mars_template_ctx->status.ParaAppointTime);
                    break;
                }
                case prop_ParaSaltGear:
                {
                    cJSON_AddNumberToObject(proot, "ParaSaltGear", mars_template_ctx->status.ParaSaltGear);
                    break;
                }
                case prop_ParaAgentGear:
                {
                    cJSON_AddNumberToObject(proot, "ParaAgentGear", mars_template_ctx->status.ParaAgentGear);
                    break;
                }
                case prop_ParaOnlySaltState:
                {
                    cJSON_AddNumberToObject(proot, "ParaOnlySaltState", mars_template_ctx->status.ParaOnlySaltState);
                    break;
                }
                case prop_ParaOnlyAgentState:
                {
                    cJSON_AddNumberToObject(proot, "ParaOnlyAgentState", mars_template_ctx->status.ParaOnlyAgentState);
                    break;
                }
                case prop_DataRunState:
                {
                    cJSON_AddNumberToObject(proot, "DataRunState", mars_template_ctx->status.DataRunState);
                    break;
                }
                case prop_DataDoorState:
                {
                    cJSON_AddNumberToObject(proot, "DataDoorState", mars_template_ctx->status.DataDoorState);
                    break;
                }
                case prop_DataLeftAppointmentTime:
                {
                    cJSON_AddNumberToObject(proot, "DataLeftAppointmentTime", mars_template_ctx->status.DataLeftAppointmentTime);
                    break;
                }
                case prop_DataLeftRunTime:
                {
                    cJSON_AddNumberToObject(proot, "DataLeftRunTime", mars_template_ctx->status.DataLeftRunTime);
                    break;
                }
                case prop_DataWashStep:
                {
                    cJSON_AddNumberToObject(proot, "DataWashStep", mars_template_ctx->status.DataWashStep);
                    break;
                }
                default:
                    break;
            }
        }
    }

    mars_template_ctx->dishwasher_reportflg = 0;
}

#endif