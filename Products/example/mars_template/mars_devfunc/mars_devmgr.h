/*** 
 * @Description  : 
 * @Author       : zhoubw
 * @Date         : 2022-06-08 10:13:54
 * @LastEditors  : zhoubw
 * @LastEditTime : 2022-11-28 14:26:24
 * @FilePath     : /tg7100c/Products/example/mars_template/mars_devfunc/mars_devmgr.h
 */

#ifndef __MARS_DEVMGR_H__
#define __MARS_DEVMGR_H__
#ifdef __cplusplus
extern "C" {
#endif

#include "cJSON.h"
#include "iot_export.h"
#include "../dev_config.h"
#include "../mars_driver/mars_uartmsg.h"
#include "../mars_driver/mars_network.h"


typedef enum CMD_TYPE{
    cmd_null = 0x00,
    cmd_get = 0x01,
    cmd_set,
    cmd_event,
    cmd_keypress,
    cmd_store,
    cmd_reqstatus,
    cmd_reqstatusack,

    cmd_ota_0A = 0x0A,
    cmd_ota_0B = 0x0B,
    
    cmd_getack = 0x0D, 
    cmd_ack,
    cmd_nak,
}M_CMD_TYPE_E;

enum MARS_DEV_EVENT_ID{
    MARS_EVENT_SEND_SWVERSION,
    MARS_EVENT_SEND_HWVERSION,
    MARS_EVENT_FACTORY_WIFITEST,
    MARS_EVENT_FACTORY_WASS,
    MARS_EVENT_FACTORY_LINKKEY,
    MARS_EVENT_FACTORY_RESET,
    MARS_EVENT_REPORT_WIFI_PROPERTY,
};

enum DEVIECE_NET_STATE  
{
    NET_STATE_NOT_CONNECTED = 0,    //0：未联网 
    NET_STATE_CONNECTED     = 1,    //1：已联网
    NET_STATE_CONNECTING    = 2     //2：配网中
};

typedef enum{
    prop_AllGet= 0x00,

    prop_ElcSWVersion = 0x01,           //控制板（显示板）软件版本号
    prop_ElcHWVersion,                  //控制板（显示板）硬件版本号
    prop_PwrSWVersion,                  //电源板软件版本号
    prop_PwrHWVersion,                  //电源板硬件版本号
    prop_SysPower,                      //系统开关
    prop_NetState,                      //联网状态
    prop_Netawss,                       //配网动作

    prop_ErrCode = 0x0A,                //警报状态
    prop_ErrCodeShow = 0x0B,            //当前显示的报警状态

#if MARS_STOVE 
    prop_LStoveStatus = 0x11,           //左灶状态(旋钮状态)
    prop_RStoveStatus,                  //右灶状态(旋钮状态)
    prop_HoodStoveLink,                 //烟灶联动
    prop_HoodLightLink,                 //烟机照明联动

    prop_RStoveTimingState,             //右灶定时状态
    prop_RStoveTimingOpera,             //右灶定时动作
    prop_RStoveTimingSet,               //右灶定时时间
    prop_RStoveTimingLeft,              //右灶定时剩余时间

    prop_LStoveTimingState,             //左灶定时状态
    prop_LStoveTimingOpera,             //左灶定时动作
    prop_LStoveTimingSet,               //左灶定时时间
    prop_LStoveTimingLeft,              //左灶定时剩余时间


    prop_HoodFireTurnOff = 0x1D,        //灶具关火
    prop_LThermocoupleState = 0x1E,     //左灶状态(左热电偶)
    prop_RThermocoupleState = 0x1F,     //右灶状态(右热电偶)
    prop_RStoveSwitch = 0x20,           //右灶通断阀
    prop_RAuxiliarySwitch=0x21,         //右灶辅助控温
    prop_RAuxiliaryTemp=0x22,           //右灶辅助控温温度
    prop_ROilTemp=0x23,                 //右灶油温
    prop_RMovePotLowHeatSwitch=0x24,    //右灶移锅小火
    prop_REnvTemp=0x25,                 //右灶环温(NTC温度)
    prop_OilTempSwitch=0x26,            //右灶油温显示开关

    prop_HoodSpeedKey = 0x30,           //烟机按键操作
    prop_HoodSpeed = 0x31,              //烟机风速
    prop_HoodLight = 0x32,              //烟机照明
    prop_HoodOffLeftTime = 0x34,        //烟机延时关机剩余时间
    prop_FsydSwitch = 0x35,             //智慧控烟
    prop_HoodSteplessSpeed = 0x36,      //智能控烟是否开启
    prop_FsydValid = 0x37,              //智能控烟是否运行
    prop_HoodOffTimer = 0x38,           //烟机延时设定时间
    prop_HoodTurnOffRemind = 0x39,      //烟机关机提醒
    prop_OilBoxState = 0x3A,            //集油盒状态
    prop_BalconyHeat = 0x3B,            //置物台加热
#endif

#if MARS_STEAMOVEN
    prop_LStOvOperation = 0x41,         //左腔蒸烤动作
    prop_LStOvState = 0x42,             //左腔工作状态
    prop_LStOvMode = 0x43,              //左腔工作模式
    prop_LStOvSetTemp = 0x44,           //左腔设定温度
    prop_LStOvRealTemp,                 //左腔实时温度
    prop_LStOvSetTimer,                 //左腔设定时间
    prop_LStOvSetTimerLeft,             //左腔剩余时间
    prop_LStOvOrderTimer,               //左腔倒计时预约
    prop_LStOvOrderTimerLeft,           //左腔倒计时剩余时间
    prop_LStOvDoorState,                //左腔箱门状态
    prop_LStOvLightState,               //左腔照明状态
    prop_LMultiStageContent,            //左腔多段程序
    prop_LMultiStageState,              //左腔多段状态
    prop_LCookbookName,                 //左腔菜谱名称
    prop_LMultiMode,                    //左腔烹饪类型
    prop_LCookbookID,                   //左腔烹饪ID
    prop_RStOvOperation,                //右腔蒸烤动作
    prop_RStOvState,                    //右腔工作状态
    prop_RStOvSetTemp,                  //右腔设定温度
    prop_RStOvRealTemp,                 //右腔实时温度
    prop_RStOvSetTimer,                 //右腔设定时间
    prop_RStOvSetTimerLeft,             //右腔剩余时间
    prop_RStOvOrderTimer,               //右腔倒计时预约
    prop_RStOvOrderTimerLeft,           //右腔倒计时剩余时间
    prop_RStOvDoorState,                //右腔箱门状态
    prop_RStOvLightState,               //右腔照明状态
    prop_RStOvMode,                     //右腔工作模式
    prop_RMultiStageContent,            //右腔多段程序
    prop_RMultiStageState,              //右腔多段状态
    prop_RCookbookName,                 //右腔菜谱名称
    prop_RMultiMode,                    //右腔烹饪类型
    prop_RCookbookID,                   //右腔菜谱ID
#endif

#if MARS_DISHWASHER
    prop_CmdWashAction = 0x41,          //洗碗动作      0-4
    prop_DataRunState,                  //运行状态      0-5 
    prop_ParaWashMode,                  //洗涤模式      1-7 
    prop_ParaAttachMode,                //附加模式
    prop_ParaDryTime,                   //烘干时间
    prop_ParaHalfWash,                  //半区洗涤      0-3
    prop_ParaAppointTime,               //预约时间      0-1440
    prop_DataWashStep,                  //洗涤进度      0-7
    prop_DataDoorState,                 //箱门状态      0-1
    prop_DataLeftAppointmentTime,       //剩余预约时间  0-1440
    prop_DataLeftRunTime,               //剩余运行时间  0-400
    prop_ParaOnlySaltState,             //软盐水状态    0-1
    prop_ParaSaltGear,                  //软水盐档位    1-6
    prop_ParaOnlyAgentState,            //亮碟剂状态    0-1
    prop_ParaAgentGear,                 //亮碟剂档位    1-3 
#endif

#if MARS_STERILIZER

#endif

    prop_ServeTime = 0x61,              //时间
    prop_ServeDate = 0x62,              //日期

#if MARS_DISPLAY
    prop_ChickenSoupDispSwitch = 0x70,      //语录显示总开关
    prop_ChickenSoupContent = 0x71,         //下发文字内容
    prop_CurrentChickenSoupContent,         //当前文字内容
    prop_BirthdayInfo,                      //下发生日信息
    prop_CurrentBirthdayInfo,               //当前生日信息
    prop_FestivalInfo,                      //节日信息
    prop_WeatherInfo,                       //天气信息
    prop_EnvTemp,                           //气温

    prop_DisplaySWVersion = 0x7E,           //头部显示板软件版本号
    prop_DisplayHWVersion = 0x7F,           //头部显示板硬件版本号
#endif

    prop_LSteamGear = 0x80,             //左腔蒸汽档位
    prop_LDoorPauseLeftTime = 0x81,     //左腔开关门暂停剩余时间
    prop_RDoorPauseLeftTime = 0x82,     //右腔开关门暂停剩余时间
    prop_BaoChaoLeftTime = 0x83,        //爆炒剩余时间

    prop_VoiceSwitchCmd     = 0xA0,     //语音开关动作
    prop_VoiceSwitchState   = 0xA1,     //语音开关状态

    prop_LocalAction = 0xF1,            //本地特定操作
    prop_FactoryResult,                 //厂测结果
    prop_NetFwVer,                      //通讯板软件版本
    prop_NetHwVer,                      //通讯板硬件版本
    prop_Mac,                           //通讯板WiFi和蓝牙MAC地址
    prop_DataReportReason,              //本次数据上报原因  0-3
    prop_Beer,                          //蜂鸣器
    prop_ota_request= 0xF9,             //ota请求
    prop_ota_response= 0xFA             //ota应答
}m_propId_e;

typedef enum MARS_NAK
{
	NAK_ERROR_CRC = 0x01,               //校验出错
	NAK_ERROR_TAILER = 0x02,            //帧尾信息与预期不符合
	NAK_ERROR_CMDVER = 0x03,            //命令版本与预期不符合
	NAK_ERROR_CMDCODE = 0x04,           //命令码类型不明
	NAK_ERROR_CMDCODE_NOSUPPORT = 0x05, //命名码类型与属性项目不匹配
	NAK_ERROR_UNKOWN_PROCODE = 0x06,    //未知的属性代码
	NAK_ERROR_PROVALUE = 0x07,          //属性值超访问
	NAK_ERROR_NOALLOWOPERATE = 0x08,    //暂时不允许操作
	NAK_ERROR_RECEIVER = 0x09,          //接收方内部异常
	NAK_ERROR_NODEFINE = 0xFF,          //未定义的错误
}m_nak_e;

typedef enum{
    FESTIVAL_NULL = 0x00,          //无节日
    SPRING_FESTIVAL,        //春节
    LANTERN_FESTIVAL,       //元宵节
    MAY_DAY,                //劳动节
    DRAGONBOAT_FESTIVAL,    //端午节
    MIDAUTUMN_FESTIVAL,     //中秋节
    NATIONAL_DAY,           //国庆节
    NEW_YEAR,               //元旦
    CHINESENEWYEARS_EVE,    //除夕
}m_festival_e;

typedef enum{
    SUNNY = 0x01,       //晴天
    OVERCAST,           //阴天
    CLOUDY,             //多云
    HEAVY_RAIN,         //大雨
    MODERATE_RAIN,      //中雨
    LIGHT_RAIN,         //小雨
    THUNDERSTORM,       //雷雨
    WINDY,              //大风
    SNOW,               //雪
    FOG,                //雾
    SLEET,              //雨夹雪
}m_weather_e;

#pragma pack(1)

#if MARS_STEAMOVEN
typedef struct{
    uint8_t stage_index;            //步骤编号
    uint8_t remind;                //是否提醒
    uint8_t workmode;               //工作模式
    uint16_t set_temper;             //设定温度
    uint16_t set_time;               //设定时间
    uint16_t steam_time;             //蒸汽盘外圈时间
    uint16_t fan_time;               //侧面风机时间
    uint8_t water_time;             //注水时间
    uint8_t remind_len;             //提醒内容长度
    uint8_t remind_buf[44];         //提醒内容
}mars_multiStage_st;

typedef struct{
    uint8_t stage_sum;              //有效多段总数
    mars_multiStage_st stage[3];    //
}mars_multiStageContent_st;

typedef struct _multi_stage_state{
    uint8_t stage_sum;              //有效多段总数
    uint8_t current_state;          //当前步骤
    uint8_t remind;                //是否提醒
    uint8_t remind_len;            //提醒内容长度
    char remind_buf[44];        //提醒内容
}mars_multiStageState_st;

typedef struct text
{
    uint8_t len;
    uint8_t buf[40];
}mars_text_st;

#endif

#if MARS_DISPLAY

typedef struct{
    uint8_t msg_len;
    uint8_t msg_buf[16];
}mars_displayMsg_st;

typedef struct{
    uint8_t process;
    uint8_t month;
    uint8_t day;
}mars_displayBirthday_st;

#endif

typedef struct _DEV_STATUS{
    uint8_t ElcSWVersion;            //控制板（显示板）软件版本号
    uint8_t ElcHWVersion;            //控制板（显示板）硬件版本号
    uint8_t PwrSWVersion;            //电源板软件版本号
    uint8_t PwrHWVersion;            //电源板硬件版本号
    uint8_t SysPower;                   //系统开关
    uint8_t NetState;                   //联网状态
    uint8_t Netawss;                    //配网动作

    uint32_t ErrCode;                    //警报状态
    uint8_t ErrCodeShow;                //当前显示的报警状态

#if MARS_STOVE
    uint8_t LStoveStatus;               //左灶状态(旋钮状态)
    uint8_t RStoveStatus;               //右灶状态(旋钮状态)
    uint8_t HoodStoveLink;              //烟灶联动
    uint8_t HoodLightLink;              //烟机照明联动
    uint8_t RStoveTimingState;          //右灶定时状态
    uint8_t RStoveTimingOpera;          //右灶定时动作
    uint8_t RStoveTimingSet;            //右灶定时时间
    uint8_t RStoveTimingLeft;           //右灶定时剩余时间

    uint8_t LStoveTimingState;          //左灶定时状态
    uint8_t LStoveTimingOpera;          //左灶定时动作
    uint8_t LStoveTimingSet;            //左灶定时时间
    uint8_t LStoveTimingLeft;           //左灶定时剩余时间
    
    uint8_t HoodFireTurnOff;            //灶具关火
    uint8_t LThermocoupleState;         //左灶状态(左热电偶)
    uint8_t RThermocoupleState;         //右灶状态(右热电偶)
    uint8_t RStoveSwitch;               //右灶通断阀

    uint8_t RAuxiliarySwitch;           //右灶辅助控火开关
    int16_t RAuxiliaryTemp;             //右灶辅助控温温度
    uint8_t RAuxiliaryBeer;             //右灶辅助控火是否蜂鸣
    int16_t ROilTemp;                   //右灶油温
    uint8_t RMovePotLowHeatSwitch;      //右灶移锅小火开关
    uint8_t OilTempSwitch;              //右灶油温显示开关
    uint8_t CookingCurveSwitch;         //右灶烹饪曲线开关(仅在wifi板处理)
    int16_t REnvTemp;                   //右灶环境温度(NTC)
    int16_t LOilTemp;                   //左灶油温
    int16_t LEnvTemp;                   //左灶环境温度(NTC)
    uint16_t RMovePotLowHeatOffTime;    //右灶移锅小火熄火时间

    uint8_t HoodSpeed;                  //烟机风速
    uint8_t HoodLight;                  //烟机照明
    uint16_t HoodOffLeftTime;           //烟机延时关机剩余时间
    uint8_t FsydSwitch;                 //智慧控烟开关：常开与常闭
    uint8_t FsydStart;                  //智能排烟是否开启
    uint8_t FsydValid;                  //智能排烟是否生效
    uint8_t FsydOneExit;                //智能排烟是否单次退出
    uint8_t HoodSteplessSpeed;          //烟机无极风速
    uint16_t HoodOffTimer;              //烟机延时设定时间
    uint8_t HoodTurnOffRemind;          //烟机关机提醒
    uint8_t fsydHoodMinSpeed;           //风随烟动最小档（不是物模型属性，做逻辑处理使用）
    
    uint8_t OilBoxState;                //集油盒状态
    uint8_t BalconyHeatState;           //置物台加热
#endif

#if MARS_STEAMOVEN
    uint8_t LStOvOperation;             //左腔蒸烤动作
    uint8_t LStOvState;                 //左腔工作状态
    uint8_t LStOvMode;                  //左腔工作模式
    uint8_t LSteamGear;                 //左腔蒸汽档位
    int16_t LStOvSetTemp;               //左腔设定温度
    int16_t LStOvRealTemp;              //左腔实时温度
    uint16_t LStOvSetTimer;             //左腔设定时间
    uint16_t LStOvSetTimerLeft;         //左腔剩余时间
    uint16_t LStOvOrderTimer;           //左腔倒计时预约
    uint16_t LStOvOrderTimerLeft;       //左腔倒计时剩余时间
    uint8_t LStOvDoorState;             //左腔箱门状态
    uint8_t LStOvLightState;            //左腔照明状态
  
    mars_multiStageContent_st LMultiStageContent;   //左腔多段程序
    mars_multiStageState_st LMultiStageState;       //左腔多段状态
    mars_text_st LCookbookName;                     //左腔菜谱名称
    uint8_t LMultiMode;                 //左腔烹饪类型
    uint32_t LCookbookID;               //左腔烹饪ID

    uint8_t RStOvOperation;             //右腔蒸烤动作
    uint8_t RStOvState;                 //右腔工作状态
    int16_t RStOvSetTemp;               //右腔设定温度
    int16_t RStOvRealTemp;              //右腔实时温度
    uint16_t RStOvSetTimer;             //右腔设定时间
    uint16_t RStOvSetTimerLeft;         //右腔剩余时间
    uint16_t RStOvOrderTimer;           //右腔倒计时预约
    uint16_t RStOvOrderTimerLeft;       //右腔倒计时剩余时间
    uint8_t RStOvDoorState;             //右腔箱门状态
    uint8_t RStOvLightState;            //右腔照明状态
    uint8_t RStOvMode;                  //右腔工作模式

    // mars_multiStageContent_st RMultiStageContent;   //右腔多段程序
    // mars_multiStageState_st RMultiStageState;       //右腔多段状态
    // mars_text_st RCookbookName;                     //右腔菜谱名称
    uint8_t RMultiMode;                 //右腔烹饪类型
    // uint32_t RCookbookID;               //右腔菜谱ID
#endif

#if MARS_DISHWASHER
    uint8_t CmdWashAction;              //洗碗动作      0-4
    uint8_t DataRunState;               //运行状态      0-5 
    uint8_t ParaWashMode;               //洗涤模式      1-7 
    uint8_t ParaAttachMode;             //附加模式
    uint16_t ParaDryTime;               //烘干时间
    uint8_t ParaHalfWash;               //半区洗涤      0-3
    uint16_t ParaAppointTime;           //预约时间      0-1440
    uint8_t DataWashStep;               //洗涤进度      0-7
    uint8_t DataDoorState;              //箱门状态      0-1
    uint16_t DataLeftAppointmentTime;   //剩余预约时间  0-1440
    uint16_t DataLeftRunTime;           //剩余运行时间  0-400
    uint8_t ParaOnlySaltState;          //软盐水状态    0-1
    uint8_t ParaSaltGear;               //软水盐档位    1-6
    uint8_t ParaOnlyAgentState;         //亮碟剂状态    0-1
    uint8_t ParaAgentGear;              //亮碟剂档位    1-3 
#endif
    
#if MARS_DISPLAY
    uint8_t ChickenSoupDispSwitch;      //语录显示总开关
    mars_displayMsg_st ChickenSoupContent[5];        //下发文字内容
    mars_displayMsg_st CurrentChickenSoupContent[5];  //当前文字内容
    mars_displayBirthday_st BirthdayInfo[5];               //下发生日信息
    mars_displayBirthday_st CurrentBirthdayInfo[5];        //当前生日信息
    m_festival_e FestivalInfo;               //节日信息
    m_weather_e WeatherInfo;                //天气信息
    uint8_t EnvTemp;                    //气温

    uint8_t DisplaySWVersion;           //头部显示板软件版本号
    uint8_t DisplayHWVersion;           //头部显示板硬件版本号
#endif

    uint8_t DataReportReason;           //上报触发源
    uint8_t WifiSWVersion[3];           //通讯板软件版本号
    uint32_t CurKeyvalue;
    uint8_t  OTAbyAPP;
    uint8_t  TokenRptFlag;
} dev_status_t;

typedef struct {
    int master_devid;
    int cloud_connected;
    int master_initialized;
    int bind_notified;
    char product_key[PRODUCT_KEY_LEN + 1];
    char product_secret[PRODUCT_SECRET_LEN + 1];
    char device_name[DEVICE_NAME_LEN + 1];
    char device_secret[DEVICE_SECRET_LEN + 1];
    char pidStr[9];
    char macStr[20];
    uint16_t common_reportflg;          //通用属性上报标记
    uint64_t display_reportflg;         //显示板属性上报标记
    uint64_t stove_reportflg;           //集成灶属性上报标记
    uint64_t steamoven_reportflg;       //蒸烤箱属性上报标记
    uint64_t dishwasher_reportflg;      //洗碗机属性上报标记
    uint64_t steamoven_spec_reportflg;  //蒸烤特殊类上报标记
    uint8_t ota_reboot;             //ota复位标记
    dev_status_t status;            //设备状态
} mars_template_ctx_t;
#pragma pack()

#define PROP_SYS_BEGIN          (prop_ElcSWVersion)
#define PROP_SYS_END            (prop_ErrCodeShow)

#define PROP_INTEGRATED_STOVE_BEIGN (prop_LStoveStatus)
#define PROP_INTEGRATED_STOVE_END   (prop_BalconyHeat)

#if (MARS_STEAMOVEN)
#define PROP_PARA_BEGIN         (prop_LStOvOperation)
#define PROP_PARA_END           (prop_RCookbookID)
#endif

#if (MARS_DISHWASHER)
#define PROP_PARA_BEGIN         (prop_CmdWashAction)
#define PROP_PARA_END           (prop_ParaAgentGear)
#endif

#define PROP_SERVE_BEGIN        (prop_ServeTime)
#define PROP_SERVE_END          (prop_ServeDate)

#define PROP_SPECIAL_BEGIN      (prop_LocalAction)
#define PROP_SPECIAL_END        (prop_DataReportReason)

#define PROP_CTRL_LOG_BEGIN      0xC0
#define PROP_CTRL_LOG_END        0xC8

typedef enum device_event{
    //集成灶故障事件推送
    EVENT_FAULT_RECOVERY            = 0,
    EVENT_FAULT_OCCUR_1             = 1,
    EVENT_FAULT_OCCUR_99            = 99,

    //蒸烤箱左腔事件推送
    EVENT_LEFT_STOV_ORDER_SATART    = 100,
    EVENT_LEFT_STOV_ORDER_FINISH    = 101,
    EVENT_LEFT_STOV_PREHEAT_SATART  = 102,
    EVENT_LEFT_STOV_PREHEAT_FINISH  = 103,
    EVENT_LEFT_STOV_COOK_SATART     = 104,
    EVENT_LEFT_STOV_COOK_FINISH     = 105,

    //蒸烤箱右腔事件推送
    EVENT_RIGHT_STOV_ORDER_SATART   = 200,
    EVENT_RIGHT_STOV_ORDER_FINISH   = 101,
    EVENT_RIGHT_STOV_PREHEAT_SATART = 102,
    EVENT_RIGHT_STOV_PREHEAT_FINISH = 103,
    EVENT_RIGHT_STOV_COOK_SATART    = 104,
    EVENT_RIGHT_STOV_COOK_FINISH    = 105,

    //左灶具事件推送
    EVENT_LEFT_STOVE_TIMER_SATART   = 300,
    EVENT_LEFT_STOVE_TIMER_FINISH   = 301,
    EVENT_LEFT_STOVE_OILBOX_REMIND  = 302,

    //右灶具事件推送
    EVENT_RIGHT_STOVE_TIMER_SATART  = 400,
    EVENT_RIGHT_STOVE_TIMER_FINISH  = 401,
    EVENT_RIGHT_STOVE_OILBOX_REMIND = 402
}device_event_t;


/*** 
 * @brief 获取当前设备状态
 * @return {*}
 */
mars_template_ctx_t *mars_dm_get_ctx(void);

/*** 
 * @brief 串口消息上报处理函数
 * @param {uartmsg_que_t} *msg
 * @return {*}
 */
bool mars_uart_prop_process(uartmsg_que_t *msg);


/*** 
 * @brief 属性改变上报至云端
 * @param {char*} msg_seq
 * @param {char} *
 * @return {*}
 */
void mars_property_data(char* msg_seq, char **srt_out);
// int mars_set_process(cJSON *root, cJSON *item);
int Mars_property_set_callback(cJSON *root, cJSON *item, void *msg);
void Mars_property_get_callback(char *property_name, cJSON *response);
void mars_store_netstatus();
void mars_store_voicestatus();

void mars_devmgr_afterConnect(void);

#ifdef __cplusplus
}
#endif
#endif