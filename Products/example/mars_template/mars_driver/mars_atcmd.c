/*
 * @Description  : 
 * @Author       : zhoubw
 * @Date         : 2022-07-25 10:21:49
 * @LastEditors  : zhoubw
 * @LastEditTime : 2022-10-28 09:54:53
 * @FilePath     : /alios-things/Products/example/mars_template/mars_driver/mars_atcmd.c
 */

#include <aos/aos.h>
#include <string.h>

#include "iot_export.h"
#include <netmgr.h>
#include <hal/wifi.h>

#include "mars_atcmd.h"
#include "app_entry.h"
#include "../mars_devfunc/mars_devmgr.h"

static bool g_atcmd_status = false;
static uint8_t g_flash_save_flag = 0;

static aos_task_t task_udp_send;

bool M_atcmd_getstatus(void)
{    
    return g_atcmd_status;
}

void M_atcmd_setstatus(bool set_status)
{
    g_atcmd_status = set_status;
}

int AtCmd_BaseInfo(char* pcAtCmd, char* pcAtValue, char* pcRetValue, unsigned int* piRetBuffLen, int *iRebootEn)
{
	if(!strcmp(pcAtValue, "?"))
	{
        int len = 0;
        char product_key[PRODUCT_KEY_LEN + 1] = {0};
        char product_secret[PRODUCT_SECRET_LEN + 1] = {0};
        char device_name[DEVICE_NAME_LEN + 1] = {0};
        char device_secret[DEVICE_SECRET_LEN + 1] = {0};
		char version[50] = {0};

        len = PRODUCT_KEY_LEN + 1;
        aos_kv_get("linkkit_product_key", product_key, &len);

        len = PRODUCT_SECRET_LEN + 1;
        aos_kv_get("linkkit_product_secret", product_secret, &len);

        len = DEVICE_NAME_LEN + 1;
        aos_kv_get("linkkit_device_name", device_name, &len);

        len = DEVICE_SECRET_LEN + 1;
        aos_kv_get("linkkit_device_secret", device_secret, &len);

		sprintf(version, "%s", aos_get_app_version());

		sprintf(pcRetValue, "AT+%s=[%s][%s][%s][%s][%s]\r\n", pcAtCmd, 
			device_name, device_secret, product_key, product_secret, version);
	}
	return 0;
}

int AtCmd_SysVer(char* pcAtCmd, char* pcAtValue, char* pcRetValue, unsigned int* piRetBuffLen, int *iRebootEn)
{
	if(!strcmp(pcAtValue, "?"))
	{
		sprintf(pcRetValue, "AT+%s=%s\r\n", pcAtCmd, aos_get_app_version());
	}
	return 0;
}

int AtCmd_VoiceVer(char* pcAtCmd, char* pcAtValue, char* pcRetValue, unsigned int* piRetBuffLen, int *iRebootEn)
{
	if(!strcmp(pcAtValue, "?"))
	{
        extern unsigned char* xm_voice_get_voice_version(void);
		sprintf(pcRetValue, "AT+%s=%s\r\n", pcAtCmd, xm_voice_get_voice_version());
	}
	return 0;
}

void AtCmd_ProductId(char* pcAtCmd, char* pcAtValue, char* pcRetValue, unsigned int* piRetBuffLen, unsigned char ucChn)
{
	if(!strcmp(pcAtValue, "?"))
	{
        char pidStr[9] = { 0 };
		int len = sizeof(pidStr);

        if (0 == aos_kv_get("linkkit_product_id", pidStr, &len)) {
			sprintf(pcRetValue, "AT+%s=%s\r\n", pcAtCmd, pidStr);
        }
	}
	else
	{
		if(aos_kv_set("linkkit_product_id", pcAtValue, strlen(pcAtValue), 1) == 0){
			sprintf(pcRetValue, "AT+%s=OK\r\n", pcAtCmd);
		}else{
			sprintf(pcRetValue, "AT+%s=FAIL\r\n", pcAtCmd);
		}
	}
	return;
}

void AtCmd_ProductKey(char* pcAtCmd, char* pcAtValue, char* pcRetValue, unsigned int* piRetBuffLen, unsigned char ucChn)
{
	if(!strcmp(pcAtValue, "?"))
	{
        char product_key[PRODUCT_KEY_LEN + 1] = { 0 };
        int len = PRODUCT_KEY_LEN + 1;

		if (0 == aos_kv_get("linkkit_product_key", product_key, &len)){
			sprintf(pcRetValue, "AT+%s=%s\r\n", pcAtCmd, product_key);
		}
	}
	else
	{
		if(aos_kv_set("linkkit_product_key", pcAtValue, strlen(pcAtValue), 1) == 0){
			sprintf(pcRetValue, "AT+%s=OK\r\n", pcAtCmd);
		}else{
			sprintf(pcRetValue, "AT+%s=FAIL\r\n", pcAtCmd);
		}
	}
	return;
}
void AtCmd_ProductSecret(char* pcAtCmd, char* pcAtValue, char* pcRetValue, unsigned int* piRetBuffLen, unsigned char ucChn)
{
	if(!strcmp(pcAtValue, "?"))
	{
        char product_secret[PRODUCT_SECRET_LEN + 1] = { 0 };
		int len = PRODUCT_SECRET_LEN + 1;

        if(0 == aos_kv_get("linkkit_product_secret", product_secret, &len)){
			sprintf(pcRetValue, "AT+%s=%s\r\n", pcAtCmd, product_secret);
		}
	}
	else
	{
		if(aos_kv_set("linkkit_product_secret", pcAtValue, strlen(pcAtValue), 1) == 0){
			sprintf(pcRetValue, "AT+%s=OK\r\n", pcAtCmd);
		}else{
			sprintf(pcRetValue, "AT+%s=FAIL\r\n", pcAtCmd);
		}
	}
	return;
}
void AtCmd_DeviceName(char* pcAtCmd, char* pcAtValue, char* pcRetValue, unsigned int* piRetBuffLen, unsigned char ucChn)
{
	if(!strcmp(pcAtValue, "?"))
	{
        char device_name[DEVICE_NAME_LEN + 1] = { 0 };
		int len = DEVICE_NAME_LEN + 1;
        if (0 == aos_kv_get("linkkit_device_name", device_name, &len)){
			sprintf(pcRetValue, "AT+%s=%s\r\n", pcAtCmd, device_name);
		}
	}
	else
	{
		if(aos_kv_set("linkkit_device_name", pcAtValue, strlen(pcAtValue), 1) == 0){
			sprintf(pcRetValue, "AT+%s=OK\r\n", pcAtCmd);
		}else{
			sprintf(pcRetValue, "AT+%s=FAIL\r\n", pcAtCmd);
		}
	}
	return;
}
void AtCmd_DeviceSecret(char* pcAtCmd, char* pcAtValue, char* pcRetValue, unsigned int* piRetBuffLen, unsigned char ucChn)
{
	if(!strcmp(pcAtValue, "?"))
	{
        char device_secret[DEVICE_SECRET_LEN + 1] = { 0 };
		int len = DEVICE_SECRET_LEN + 1;
        if (0 == aos_kv_get("linkkit_device_secret", device_secret, &len)){
			sprintf(pcRetValue, "AT+%s=%s\r\n", pcAtCmd, device_secret);
		}
	}
	else
	{
		if(aos_kv_set("linkkit_device_secret", pcAtValue, strlen(pcAtValue), 1) == 0){
			sprintf(pcRetValue, "AT+%s=OK\r\n", pcAtCmd);
		}else{
			sprintf(pcRetValue, "AT+%s=FAIL\r\n", pcAtCmd);
		}
	}
	return;
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

static void m_hexstr2bin(const char *macstr, uint8_t *mac, int len)
{
    int i;
    for (i = 0; i < len && macstr[2 * i]; i++) {
        mac[i] = hex(macstr[2 * i]) << 4;
        mac[i] |= hex(macstr[2 * i + 1]);
    }
}

void AtCmd_MacSet(char* pcAtCmd, char* pcAtValue, char* pcRetValue, unsigned int* piRetBuffLen, int *iRebootEn)
{
	if(!strcmp(pcAtValue, "?"))
	{
    	uint8_t mac[6] = {0};
		char mac_str[20] = {0};
		hal_wifi_get_mac_addr(NULL, mac);
        sprintf(mac_str, "%02x%02x%02x%02x%02x%02x",
				mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		sprintf(pcRetValue, "AT+%s=%s\r\n", pcAtCmd, mac_str);
	}
	else
	{
	    uint8_t mac[6] = {0};
		m_hexstr2bin(pcAtValue, mac, 6);
#if 0
        if (0 == hal_wifi_set_mac_addr(NULL, mac)){
			sprintf(pcRetValue, "AT+%s=OK\r\n", pcAtCmd);
		}
#else
        // extern uint8_t mfg_media_is_macaddr_slot_empty(uint8_t reload);
        // mfg_media_is_macaddr_slot_empty(1);

        extern int8_t mfg_media_write_macaddr_pre_with_lock(uint8_t mac[6],uint8_t program);
        if(0 == mfg_media_write_macaddr_pre_with_lock(mac, 1)){
			sprintf(pcRetValue, "AT+%s=OK\r\n", pcAtCmd);
		}else{
			if(0 == aos_kv_set("USER_MAC", mac, sizeof(mac), 1)){
				sprintf(pcRetValue, "AT+%s=OK\r\n", pcAtCmd);
			}
		}
#endif
	}
	
	return;
}

void Atcmd_FactorySet(char* pcAtCmd, char* pcAtValue, char* pcRetValue, unsigned int* piRetBuffLen, int *iRebootEn)
{
	if(!strcmp(pcAtValue, "?"))
	{	
		char factory[32] = {0};
		int temp_len = 32;
		aos_kv_get("user_fact", factory, &temp_len);
		sprintf(pcRetValue, "AT+%s=%s\r\n", pcAtCmd, factory);
	}
	else
	{
		aos_kv_set("user_fact", pcAtValue, 32, 1);
		sprintf(pcRetValue, "AT+%s=OK\r\n", pcAtCmd);
	}
	return;
}

#define SCAN_STATE_IDLE         (0)
#define SCAN_STATE_BUSY         (1)
#define SCAN_STATE_DONE         (2)
static int test_scan_state = SCAN_STATE_IDLE;
static void scan_done_cb(void *data, void *param) 
{
  test_scan_state = SCAN_STATE_DONE;
}

void Atcmd_Rfget(char* pcAtCmd, char* pcAtValue, char* pcRetValue, unsigned int* piRetBuffLen, int *iRebootEn)
{
    /* scan wifi */
#if 0
    ap_scan_info_t scan_result;
    int ap_scan_result = -1;
    // start ap scanning for default 3 seconds
    memset(&scan_result, 0, sizeof(ap_scan_info_t));
    ap_scan_result = awss_apscan_process(NULL, pcAtValue, &scan_result);
	if ( (ap_scan_result == 0) && (scan_result.found) ) {
		sprintf(pcRetValue, "AT+%s=%d\r\n", pcAtCmd, scan_result.rssi);
    }
	return;
#endif

    LOGI("mars", "AT指令: 开始扫描热点%s", pcAtValue);
    test_scan_state = SCAN_STATE_IDLE;
    wifi_mgmr_scan(NULL, scan_done_cb);//trigger the scan
    while (SCAN_STATE_DONE != test_scan_state) 
    {
        LOGW("mars", "wifi_scan: 扫描中......");
        aos_msleep(1000);
    }

    wifi_mgmr_ap_item_t ap_itme = {0x00};
    if (wifi_mgmr_scan_ap(pcAtValue, &ap_itme) == 0)
    {
        sprintf(pcRetValue, "AT+%s=%d\r\n", pcAtCmd, ap_itme.rssi);
        LOGW("mars", "AT指令: %s扫描成功 (信号强度=%d dBm)", pcAtValue, ap_itme.rssi);        
    }
    else
    {
        ap_itme.rssi = -90;
        sprintf(pcRetValue, "AT+%s=%d\r\n", pcAtCmd, ap_itme.rssi);
        LOGE("mars", "AT指令: %s扫描失败!!!!!!", pcAtValue);
    }
    test_scan_state = SCAN_STATE_IDLE;
}

void Atcmd_RfTest(char* pcAtCmd, char* pcAtValue, char* pcRetValue, unsigned int* piRetBuffLen, int *iRebootEn)
{
#if 0
    /* scan wifi */
    ap_scan_info_t scan_result = {0};
    int ap_scan_result = -1;
	char *result;
	result = strsep(&pcAtValue, ",");
	if (NULL == result)
    {
        LOGE("mars", "Atcmd_RfTest fail (strsep)");
		return;
	}
    // start ap scanning for default 3 seconds
    memset(&scan_result, 0, sizeof(ap_scan_info_t));
    LOGW("mars", "开始扫描热点 %s", result);
    hal_wifi_suspend_station(NULL);
    ap_scan_result = awss_apscan_process(NULL, result, &scan_result);
	if ( (ap_scan_result == 0) && (scan_result.found) ) 
    {
		// result = strtok(NULL, ",");
		int rssi_set = atoi(pcAtValue);
		if(scan_result.rssi >= rssi_set)
        {
			sprintf(pcRetValue, "AT+%s=OK\r\n", pcAtCmd);
		}
        else
        {
			sprintf(pcRetValue, "AT+%s=FAIL\r\n", pcAtCmd);
		}
    }
    else
    {
        LOGE("mars", "Atcmd_RfTest fail (awss_apscan_process)");
    }
	return;
#endif

        /* scan wifi */
    ap_scan_info_t scan_result = {0};
    int ap_scan_result = -1;
	char *result;
	result = strsep(&pcAtValue, ",");
	if (NULL == result)
    {
        LOGE("mars", "Atcmd_RfTest fail (strsep)");
		return;
	}

    // start ap scanning for default 3 seconds
    LOGI("mars", "AT指令: 开始扫描热点%s", result);
    test_scan_state = SCAN_STATE_IDLE;
    wifi_mgmr_scan(NULL, scan_done_cb);//trigger the scan
    while (SCAN_STATE_DONE != test_scan_state) 
    {
        LOGW("mars", "wifi_scan: 扫描中......");
        aos_msleep(1000);
    }

    wifi_mgmr_ap_item_t ap_itme = {0x00};
    if (wifi_mgmr_scan_ap(result, &ap_itme) == 0)
    {
		// result = strtok(NULL, ",");
		int rssi_set = atoi(pcAtValue);
        LOGW("mars", "实测信号强度=%ddBm,  门槛信号强度=%ddBm", ap_itme.rssi, rssi_set);
		if(ap_itme.rssi >= rssi_set)
        {
			sprintf(pcRetValue, "AT+%s=OK\r\n", pcAtCmd);
		}
        else
        {
			sprintf(pcRetValue, "AT+%s=FAIL\r\n", pcAtCmd);
		}
    }
    else
    {
        LOGE("mars", "Atcmd_RfTest fail (wifi_mgmr_scan_ap)");
    }
    test_scan_state = SCAN_STATE_IDLE;
	return;
}

void Atcmd_Netmgr(char* pcAtCmd, char* pcAtValue, char* pcRetValue, unsigned int* piRetBuffLen, int *iRebootEn)
{
	netmgr_ap_config_t config = {0};

	char *result;
	result = strsep(&pcAtValue, ",");
	if (NULL == result){
		return;
	}

	strncpy(config.ssid, result, sizeof(config.ssid) - 1);
	strncpy(config.pwd, pcAtValue, sizeof(config.pwd) - 1);

	LOG("NET mgr ssid(%s), password(%s)\r\n", config.ssid, config.pwd);
	
	netmgr_set_ap_config(&config);
	if (0 == netmgr_start(false)){
		sprintf(pcRetValue, "AT+%s=OK\r\n", pcAtCmd);
	}
	
	return;
}

char g_ip_addr[30] = {0};
int g_port = 0;

void udp_send_task(void *argv)
{
	char send_msg[50] = {0};
	static uint8_t udp_cnt=0;

	while (1)
	{    
		extern void m_udp_send(char *ip_addr, int port, char *msg);
		if (strlen(g_ip_addr)){
			sprintf(send_msg, "mars module test, cnt(%d).\r\n", udp_cnt++);
			m_udp_send(g_ip_addr, g_port, send_msg);
			if (udp_cnt == 255){
				udp_cnt = 0;
			}
		}

		aos_msleep(3000);
	}

	return;
}

void Atcmd_UDP(char* pcAtCmd, char* pcAtValue, char* pcRetValue, unsigned int* piRetBuffLen, int *iRebootEn)
{
	char *result;
	result = strsep(&pcAtValue, ",");
	if (NULL == result){
		return;
	}
	
	memset(g_ip_addr, 0, sizeof(g_ip_addr));
	strncpy(g_ip_addr, result, sizeof(g_ip_addr));
	g_port = atoi(pcAtValue);
	LOG("udp set ip_addr(%s), port(%d)\r\n", g_ip_addr, g_port);

    aos_task_new_ext(&task_udp_send, "atcmd udp send", udp_send_task, NULL, 1024 + 512, AOS_DEFAULT_APP_PRI - 1);

	sprintf(pcRetValue, "AT+%s=OK\r\n", pcAtCmd);

	return;
}

void Atcmd_UART(char* pcAtCmd, char* pcAtValue, char* pcRetValue, unsigned int* piRetBuffLen, int *iRebootEn)
{
	static uint8_t uart_cnt=0;

	sprintf(pcRetValue, "AT+%s=%d\r\n", pcAtCmd, uart_cnt++);

	if (uart_cnt == 255){
		uart_cnt = 0;
	}

	return;
}

int Distinguish_AtCmd(char* pcAtCmd, char* pcAtValue, char* pcRetValue, unsigned int* piRetBuffLen, int *iRebootEn)
{
	if(!strcmp(pcAtCmd, "BASEINFO"))
	{
		AtCmd_BaseInfo(pcAtCmd, pcAtValue, pcRetValue, piRetBuffLen, iRebootEn);
	}
	if(!strcmp(pcAtCmd, "APPVER"))
	{
		AtCmd_SysVer(pcAtCmd, pcAtValue, pcRetValue, piRetBuffLen, iRebootEn);
	}
    else if(!strcmp(pcAtCmd, "VOICEVER"))
	{
		AtCmd_VoiceVer(pcAtCmd, pcAtValue, pcRetValue, piRetBuffLen, iRebootEn);
	}
	else if(!strcmp(pcAtCmd, "PID"))
	{
		AtCmd_ProductId(pcAtCmd, pcAtValue, pcRetValue, piRetBuffLen, 0);
	}
	else if(!strcmp(pcAtCmd, "PKEY"))
	{
		AtCmd_ProductKey(pcAtCmd, pcAtValue, pcRetValue, piRetBuffLen, 0);
	}
	else if(!strcmp(pcAtCmd, "PSECRET"))
	{
		AtCmd_ProductSecret(pcAtCmd, pcAtValue, pcRetValue, piRetBuffLen, 0);
	}
	else if(!strcmp(pcAtCmd, "DEVNAME"))
	{
		AtCmd_DeviceName(pcAtCmd, pcAtValue, pcRetValue, piRetBuffLen, 0);
	}
	else if(!strcmp(pcAtCmd, "DEVSECRET"))
	{
		AtCmd_DeviceSecret(pcAtCmd, pcAtValue, pcRetValue, piRetBuffLen, 0);
	}
	else if(!strcmp(pcAtCmd, "RST"))
	{
		if(1 == atoi(pcAtValue))
		{
			*iRebootEn = 1;   ////1
			sprintf(pcRetValue, "AT+%s=OK\r\n", pcAtCmd);
		}
	}
	else if(!strcmp(pcAtCmd, "DEVMAC"))
	{
		AtCmd_MacSet(pcAtCmd, pcAtValue, pcRetValue, piRetBuffLen, iRebootEn);
	}
	else if(!strcmp(pcAtCmd, "FACTORY")){
		Atcmd_FactorySet(pcAtCmd, pcAtValue, pcRetValue, piRetBuffLen, iRebootEn);
	}
	else if (!strcmp(pcAtCmd, "RFGET")){
		Atcmd_Rfget(pcAtCmd, pcAtValue, pcRetValue, piRetBuffLen, iRebootEn);
	}
	else if (!strcmp(pcAtCmd, "RFTEST")){
		Atcmd_RfTest(pcAtCmd, pcAtValue, pcRetValue, piRetBuffLen, iRebootEn);
	}
	else if (!strcmp(pcAtCmd, "NETMGR")){
		Atcmd_Netmgr(pcAtCmd, pcAtValue, pcRetValue, piRetBuffLen, iRebootEn);
	}
	else if (!strcmp(pcAtCmd, "UDP")){
		Atcmd_UDP(pcAtCmd, pcAtValue, pcRetValue, piRetBuffLen, iRebootEn);
	}
	else if (!strcmp(pcAtCmd, "UART")){
		Atcmd_UART(pcAtCmd, pcAtValue, pcRetValue, piRetBuffLen, iRebootEn);
	}
	else if(!strcmp(pcAtCmd, "QUIT")){
		if(1 == atoi(pcAtValue))
		{
			g_atcmd_status = false;
			sprintf(pcRetValue, "AT+%s=OK\r\n", pcAtCmd);
			do_awss_reset();
		}
	}

	*piRetBuffLen = strlen(pcRetValue);
	LOGI("mars", "AT应答: %s", pcRetValue);
	return 0;
}

int Execute_AtCmd(char* pcAtCmd, char* pcAtValue)
{
	if(pcAtCmd && pcAtValue)
	{
		int iRebootEn = 0;
		char cRetBuff[256] = {0};
		unsigned int iRetBuffLen = 0;

        //LOGW("mars", "指令=%s(%d)  内容=%s(%d)", pcAtCmd, strlen(pcAtCmd), pcAtValue, strlen(pcAtValue));
		
		if(strlen(pcAtValue) > 0)
		{
			if(0 == Distinguish_AtCmd(pcAtCmd, pcAtValue, cRetBuff, &iRetBuffLen, &iRebootEn))
			{
				if(iRetBuffLen > 0)
				{
					Mars_uart_send(1, cRetBuff, iRetBuffLen);
                    //LOGW("mars", "Execute_AtCmd success");	
					if(1 == iRebootEn)
					{
						aos_msleep(200);
    					HAL_Reboot();
					}
					return 0;
				}
                else
                {
                    LOGE("mars", "iRetBuffLen fail!!!");
                }
			}
            else
            {
                LOGE("mars", "Distinguish_AtCmd fail!!!");
            }
		}

		sprintf(cRetBuff, "AT+%s=ERROR\r\n", pcAtCmd);
		iRetBuffLen = strlen(cRetBuff);
		Mars_uart_send(1, cRetBuff, iRetBuffLen);
	}
	return 0;
}

uint32_t M_atcmd_Analyze(char *buf, uint32_t *remainlen)
{
	if (NULL == buf){
		return 0;
	}
    char *pcPoint = buf;
    char *pcHead = NULL;
    char *pcEnd = NULL;
    char *pcCmdStart = NULL;
    char *pcCmdEnd = NULL;

    // while (pcPoint)
    // {
        pcHead = NULL;
        pcEnd = NULL;
        pcCmdStart = NULL;
        pcCmdEnd = NULL;

        pcEnd = strstr(pcPoint, "\r\n");
        pcHead = strstr(pcPoint, "AT+");
        if(NULL == pcHead){
            if (pcEnd && (pcEnd == pcPoint)){
                Mars_uart_send(1, ATCMDNULLTEST, strlen(ATCMDNULLTEST));
            }
            // break;
			return 0;
        }
        if(NULL == pcEnd){
            // break;
			return 0;
        }

        if(pcEnd == (pcHead + 3)){
			Mars_uart_send(1, ATCMDCONNECTTEST, strlen(ATCMDCONNECTTEST));
			return 0;
        }

		pcCmdStart = pcHead + 3;
		pcCmdEnd = strchr(pcHead, '=');
		if((NULL == pcCmdEnd) || (pcCmdEnd <= pcCmdStart) || ((pcCmdEnd+1) >= pcEnd)){
			pcPoint = pcEnd + 2;
			// continue;
			return 0;
        }

		int cmd_len = pcCmdEnd - pcCmdStart;
		int value_len = pcEnd - pcCmdEnd - 1;
		char *atcmd = (char *)aos_malloc(cmd_len+1);
		char *atvalue = (char *)aos_malloc(value_len+1);
		if(atcmd && atvalue){
			memset(atcmd, 0, cmd_len+1);
			memset(atvalue, 0, value_len+1);
			memcpy(atcmd, pcCmdStart, cmd_len);
			memcpy(atvalue, pcCmdEnd+1, value_len);
			//LOGW("AT指令接收:  指令=%s 内容=%s", atcmd, atvalue);

			Execute_AtCmd(atcmd, atvalue);
        }

        if(atcmd){
            aos_free(atcmd);
            atcmd = NULL;
        }
        if(atvalue){
            aos_free(atvalue);
            atvalue = NULL;
        }

		//LOG("ATCMD END \r\n");
		// pcPoint = pcEnd + 1;
    // }

	// *remainlen = 0;

    return 0;
}
