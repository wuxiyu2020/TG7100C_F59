/*** 
 * @Description  : 
 * @Author       : zhoubw
 * @Date         : 2022-03-15 17:11:52
 * @LastEditors  : zhoubw
 * @LastEditTime : 2022-09-22 18:11:30
 * @FilePath     : /alios-things/Products/example/mars_template/mars_driver/mars_network.h
 */

#ifndef __MARS_NETWORK_H__
#define __MARS_NETWORK_H__
#ifdef __cplusplus
extern "C" {
#endif

#include <cJSON.h>


typedef enum {
    RECONFIGED = 0,            //reconfig with netconfig exist
    UNCONFIGED,                //awss start
    AWSS_NOT_START,            //standby mode(not start softAP)
    GOT_AP_SSID,               //connect AP successfully
    CONNECT_CLOUD_SUCCESS,     //connect cloud successfully
    CONNECT_CLOUD_FAILED,      //connect cloud failded
    CONNECT_AP_FAILED,         //connect ap failed
    CONNECT_AP_FAILED_TIMEOUT, //connect ap failed timeout
    APP_BIND_SUCCESS,          //bind sucessfully, wait cmd from APP
    FACTORY_BEGIN,             //factory test begin
    FACTORY_SUCCESS,           //factory test successfully
    FACTORY_FAILED_1,          //factory test failed for rssi < -60dbm
    FACTORY_FAILED_2,          //factory test failed
    UNKNOW_STATE
} MarsNetState;

typedef void (*mars_netchange_cb)(uint16_t net_state);
typedef int (*mars_property_set_cb)(cJSON *root, cJSON *item, void *msg);
typedef void (*mars_property_get_cb)(char *property_name, cJSON *response);

unsigned char Mars_get_awss_flag(void);
void Mars_start_wifi_awss(uint8_t discoverable_awss_mode);
int get_net_state(void);
void set_net_state(int state);
void check_net_config(void);
void check_factory_mode(void);
// void key_detect_event_task(void *arg);
void device_stop_awss(void);
void device_start_awss(uint8_t discoverable_awss_mode);

void Mars_network_init( mars_property_set_cb property_set_cb, 
                        mars_property_get_cb property_get_cb);
bool is_awss_state(void);



#ifdef __cplusplus
}
#endif
#endif