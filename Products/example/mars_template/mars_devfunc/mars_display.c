/*
 * @Description  : 
 * @Author       : zhoubw
 * @Date         : 2022-09-02 13:40:47
 * @LastEditors  : zhoubw
 * @LastEditTime : 2022-11-25 09:45:28
 * @FilePath     : /tg7100c/Products/example/mars_template/mars_devfunc/mars_display.c
 */

#include "cJSON.h"

#include "../dev_config.h"
#include "mars_display.h"
#include "mars_httpc.h"
#include "../mars_driver/mars_cloud.h"

#if MARS_DISPLAY

#define VALID_BIT(n)    ((uint64_t)1 << (n - prop_ChickenSoupDispSwitch))

#define DAYINFO_TEMPTAG     "\"temp\":"
#define DAYINFO_WEATHERTAG     "\"weatherId\":"

char weather_info[256] = {0x00};
int  weather_len = 0;
void display_dayInfoProcess(char *msg, uint16_t msg_len)
{
    if (NULL == msg){
        return;
    }
    LOGI("mars", "当前天气信息: %s", msg);

    char *temp_head = strstr(msg, DAYINFO_TEMPTAG);
    if (temp_head){
        char *temp_tail = strstr(temp_head, ",");
        if (temp_tail){
            char temp_str[10] = {0};
            memcpy(temp_str, temp_head+strlen(DAYINFO_TEMPTAG), temp_tail-temp_head-strlen(DAYINFO_TEMPTAG));
            int8_t temp = atoi(temp_str);

            char *weather_head = strstr(msg, DAYINFO_WEATHERTAG);
            if (weather_head){
                char *weather_tail = strstr(weather_head, ",");
                if (weather_head && weather_tail){
                    char weather_str[10] = {0};
                    memcpy(weather_str, weather_head+strlen(DAYINFO_WEATHERTAG), weather_tail-weather_head-strlen(DAYINFO_WEATHERTAG));
                    uint8_t weatherId = atoi(weather_str);

                    uint8_t buf[10] = {0};
                    uint16_t len = 0;
                    buf[len++] = prop_WeatherInfo;
                    buf[len++] = weatherId;
                    buf[len++] = prop_EnvTemp;
                    buf[len++] = temp; 
                    LOGI("mars", "下发当前天气信息: 天气=%d 温度=%d(1-晴 2-阴 3-多云 4-大雨 5-中雨 6-小雨 7-雷雨 8-大风 9-雪 A-雾 B-雨夹雪)", weatherId, temp);
                    Mars_uartmsg_send(cmd_set, uart_get_seq_mid(), &buf, len, 3);
                }
            }
        }

    }

    return;
}

#define STR_WEATHERGET  "{\"deviceMac\":\"%s\"}"
#define URL_DAYINFOGET "http://mcook.marssenger.com/application/weather/day"
char http_url[] = URL_DAYINFOGET;
// http_session_t http_handle = 0;

void get_mcook_weather(void *arg1, void *arg2)
{
    int ret = -1;
    // mars_template_ctx_t *t_mars_template_ctx = mars_dm_get_ctx();
    LOGI("mars", "开始拉取当前天气信息");

    parsed_url_t url = {0};

    httpc_req_t http_req = {
        .type = HTTP_GET,
        .version = HTTP_VER_1_1,
        .resource = NULL,
        .content = NULL,
        .content_len = 0,
    };

    //为加快处理速度，使用字符串打印组件json包
    char *content_data = (char *)aos_malloc(256);    
    if (content_data)
    {
        bzero(content_data, 256);

        int recv_len = mars_http_getpost(HTTP_DEFAULT, &http_url, &http_req, content_data, 256, 3000);
        if (recv_len)
        {
            // msg process
            LOGI("mars", "http获取当前天气信息: success (len = %d)", recv_len);
            memcpy(weather_info, content_data, recv_len);
            weather_len = recv_len;
            display_dayInfoProcess(content_data, recv_len);
        }
        else
        {
            LOGI("mars", "http获取当前天气信息失败: failed!!!");
        }
        
        aos_free(content_data);        
    }

    return;
}

void mars_display_uartMsgFromSlave(uartmsg_que_t *msg, 
                                mars_template_ctx_t *mars_template_ctx, 
                                uint16_t *index, bool *report_en, uint8_t *nak)
{
    switch (msg->msg_buf[(*index)])
    {
        case prop_CurrentBirthdayInfo:
        {
            //get/get-ack/event
            if (cmd_getack == msg->cmd || cmd_event == msg->cmd){
                if (5 == msg->msg_buf[((*index))+1]){
                    for(int i=0;i<5;++i){
                        uint8_t id = msg->msg_buf[(*index)+2+4*i]-1;
                        mars_template_ctx->status.CurrentBirthdayInfo[id].process = msg->msg_buf[(*index)+3+4*i]; //1：有效 0：无效
                        mars_template_ctx->status.CurrentBirthdayInfo[id].month = msg->msg_buf[(*index)+4+4*i];
                        mars_template_ctx->status.CurrentBirthdayInfo[id].day = msg->msg_buf[(*index)+5+4*i];
                    }
                }
                mars_template_ctx->display_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
                *report_en = true;
            }else{
                *nak = NAK_ERROR_CMDCODE_NOSUPPORT;
            }
            (*index)+=21;
            break;
        }
        // case prop_FestivalInfo:
        // {
        //     //set
        //     break;
        // }
        case prop_WeatherInfo:
        {
            //set
            if (cmd_getack == msg->cmd || cmd_event == msg->cmd){
                mars_template_ctx->status.WeatherInfo = msg->msg_buf[(*index)+1];
                // *report_en = true;
            }else{
                *nak = NAK_ERROR_CMDCODE_NOSUPPORT;
            }
            (*index)+=1;
            break;
        }
        case prop_DisplaySWVersion:
        {
            //get/get-ack/event
            if (cmd_getack == msg->cmd || cmd_event == msg->cmd){
                if (mars_template_ctx->status.DisplaySWVersion != msg->msg_buf[(*index)+1]){
                    mars_template_ctx->status.DisplaySWVersion = msg->msg_buf[(*index)+1];
                    extern bool mars_ota_inited(void);
                    if (mars_ota_inited()){
                        mars_ota_module_2_init(); 
                    }
                    //mars_sync_localtime();
                    //mars_display_weatherInfoGet();
                }
                mars_template_ctx->display_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
                *report_en = true;
            }else{
                *nak = NAK_ERROR_CMDCODE_NOSUPPORT;
            }
            (*index)+=1;
            break;
        }
        case prop_DisplayHWVersion:
        {
            //get/get-ack/event
            if (cmd_getack == msg->cmd || cmd_event == msg->cmd){
                mars_template_ctx->status.DisplayHWVersion = msg->msg_buf[(*index)+1];
                mars_template_ctx->display_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
                *report_en = true;
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

//云端下发设置
bool mars_display_setToSlave(cJSON *root, cJSON *item, mars_template_ctx_t *mars_template_ctx, uint8_t *buf_setmsg, uint16_t *buf_len)
{
    bool ret = false;
    if (NULL == root || NULL == mars_template_ctx || NULL == buf_setmsg || NULL == buf_len)
    {
        return false;
    }

    if ((item = cJSON_GetObjectItem(root, "BirthdayInfo")) != NULL && cJSON_IsArray(item)) 
    {
        memset(mars_template_ctx->status.BirthdayInfo, 0, 5*sizeof(mars_displayBirthday_st));
        int sum = cJSON_GetArraySize(item);
        buf_setmsg[(*buf_len)++] = prop_BirthdayInfo;
        buf_setmsg[(*buf_len)++] = sum;
        for(int i=0; i<sum; ++i)
        {
            cJSON *array_item = cJSON_GetArrayItem(item, i);
            if (!cJSON_IsNull(array_item)){
                uint8_t id = 0xff;
                cJSON *p = cJSON_GetObjectItem(array_item, "index");
                if (!cJSON_IsNull(p) && cJSON_IsNumber(p)) 
                {
                    id = (uint8_t)p->valueint;
                }
                if (id >0 && id <6)
                {
                    p = cJSON_GetObjectItem(array_item, "operate");
                    if (!cJSON_IsNull(p) && cJSON_IsNumber(p)){
                        mars_template_ctx->status.BirthdayInfo[i].process = (uint8_t)p->valueint;
                    }
                    p = cJSON_GetObjectItem(array_item, "month");
                    if (!cJSON_IsNull(p) && cJSON_IsNumber(p)){
                        mars_template_ctx->status.BirthdayInfo[i].month = (uint8_t)p->valueint;
                    }
                    p = cJSON_GetObjectItem(array_item, "day");
                    if (!cJSON_IsNull(p) && cJSON_IsNumber(p)){
                        mars_template_ctx->status.BirthdayInfo[i].day = (uint8_t)p->valueint;
                    }
                }
                buf_setmsg[(*buf_len)++] = id;
                buf_setmsg[(*buf_len)++] = mars_template_ctx->status.BirthdayInfo[i].process;
                buf_setmsg[(*buf_len)++] = mars_template_ctx->status.BirthdayInfo[i].month;
                buf_setmsg[(*buf_len)++] = mars_template_ctx->status.BirthdayInfo[i].day;
            }
        }
    }

    return ret;
}

void mars_display_changeReport(cJSON *proot, mars_template_ctx_t *mars_template_ctx)
{
    for (uint8_t index=prop_ChickenSoupDispSwitch; index<=prop_DisplayHWVersion; ++index)
    {
        if (mars_template_ctx->display_reportflg & VALID_BIT(index))
        {
            switch (index)
            {
                case prop_CurrentBirthdayInfo:
                {
                    //LOGI("mars", "mars_display_changeReport: prop_CurrentBirthdayInfo");
                    cJSON *array_item = cJSON_CreateArray();
                    if (array_item)
                    {
                        for (int index=0; index<5; ++index)
                        {
                            if (mars_template_ctx->status.CurrentBirthdayInfo[index].process)
                            {
                                cJSON *item = cJSON_CreateObject();
                                if (item)
                                {
                                    cJSON_AddNumberToObject(item, "index", index+1);
                                    cJSON_AddNumberToObject(item, "valid", mars_template_ctx->status.CurrentBirthdayInfo[index].process);
                                    cJSON_AddNumberToObject(item, "month",  mars_template_ctx->status.CurrentBirthdayInfo[index].month);
                                    cJSON_AddNumberToObject(item, "day",  mars_template_ctx->status.CurrentBirthdayInfo[index].day);
                                    cJSON_AddItemToArray(array_item, item);
                                }     
                            }
                        }
                        cJSON_AddItemToObjectCS(proot, "CurrentBirthdayInfo", array_item);
                    }

                    break;
                }

                case prop_DisplaySWVersion:
                {
                    //LOGI("mars", "mars_display_changeReport: prop_DisplaySWVersion");
                    char tmpstr[8] = {0};
                    sprintf(tmpstr, "%X.%X", \
                            (uint8_t)(mars_template_ctx->status.DisplaySWVersion >> 4), \
                            (uint8_t)(mars_template_ctx->status.DisplaySWVersion & 0x0F));
                    //LOGI("mars", "prop_DisplaySWVersion: 1");
                    cJSON_AddStringToObject(proot, "DisplaySWVersion", tmpstr);
                    //LOGI("mars", "prop_DisplaySWVersion: 2");
                    break;
                }

                case prop_DisplayHWVersion:
                {
                    //LOGI("mars", "mars_display_changeReport: prop_DisplayHWVersion");
                    char tmpstr[8] = {0};
                    sprintf(tmpstr, "%X.%X", \
                            (uint8_t)(mars_template_ctx->status.DisplayHWVersion >> 4), \
                            (uint8_t)(mars_template_ctx->status.DisplayHWVersion & 0x0F));
                    //LOGI("mars", "prop_DisplayHWVersion: 1");
                    cJSON_AddStringToObject(proot, "DisplayHWVersion", tmpstr);
                    //LOGI("mars", "prop_DisplayHWVersion: 2");
                    break;
                }

                default:
                    break;
            }
        }
    }

    mars_template_ctx->display_reportflg = 0;
}

#define WEATHER_UPDATE_INTERVAL     (40 * 60 * 1000)
#define WEATHER_SYNC_FAILED_TIMEOUT ( 5 * 3600 * 1000)
static aos_timer_t mars_weather = {.hdl = NULL};

void get_mcook_weather_timer_start(void)
{
    if (mars_weather.hdl == NULL)
    {
        LOGI("mars", "拉取天气信息 定时器启动......(周期=%d小时)", WEATHER_SYNC_FAILED_TIMEOUT/(3600 * 1000));
        aos_timer_new_ext(&mars_weather, get_mcook_weather, NULL, WEATHER_SYNC_FAILED_TIMEOUT, 1, 1);
    }
}

#endif