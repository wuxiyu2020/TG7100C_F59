/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

#ifndef __APP_ENTRY_H__
#define __APP_ENTRY_H__

#include <aos/aos.h>

typedef struct {
    int argc;
    char **argv;
}app_main_paras_t;

typedef enum {
    MARS_EVENT_AWSS_START,
    MARS_EVENT_AWSS_EXIT,
}MARS_AWSS_EVENT_ID;

typedef struct wifi_mgmr_ap_item {
    char ssid[32];
    char ssid_tail[1];//always put ssid_tail after ssid
    uint32_t ssid_len;
    uint8_t bssid[6];
    uint8_t channel;
    uint8_t auth;
    int8_t rssi;
} wifi_mgmr_ap_item_t;


int linkkit_main(void *paras);
void do_awss_reset(void);
void do_awss_reboot(void);
void do_awss(void);
void do_ble_awss(void);
void do_awss_dev_ap(void);
void stop_netmgr(void *p);
void start_netmgr(void *p);

#endif
