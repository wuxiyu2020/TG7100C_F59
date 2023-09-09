/*
 *copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <aos/aos.h>
#include <aos/yloop.h>
#include "netmgr.h"
#include "iot_export.h"
#include "iot_import.h"
#include "vendor.h"
#include "app_entry.h"
#include "device_state_manger.h"
#include "../mars_driver/mars_network.h"
#include "../mars_devfunc/mars_factory.h"
#include "../dev_config.h"
#ifdef EN_COMBO_NET
#include "combo_net.h"
#endif


#define AWSS_REBOOT_TIMEOUT     (4 * 1000)
#define AWSS_RESET_TIMEOUT      (6 * 1000)
#define KEY_PRESSED_VALID_TIME  100
#define KEY_DETECT_INTERVAL     50
#define AWSS_REBOOT_CNT         AWSS_REBOOT_TIMEOUT / KEY_DETECT_INTERVAL
#define AWSS_RESET_CNT          AWSS_RESET_TIMEOUT / KEY_DETECT_INTERVAL
#define KEY_PRESSED_CNT         KEY_PRESSED_VALID_TIME / KEY_DETECT_INTERVAL

#if 0
void key_detect_event_task(void *arg)
{
    int nCount = 0, awss_mode = 0;
    int timeout = (AWSS_REBOOT_CNT < AWSS_RESET_TIMEOUT)? AWSS_REBOOT_CNT : AWSS_RESET_TIMEOUT;

    while (1) {
        if (!product_get_key()) {
            nCount++;
            LOG("nCount :%d", nCount);
        } else {
            if (nCount >= KEY_PRESSED_CNT && nCount < timeout) {
                if (product_get_switch() == ON) {
                    product_set_switch(OFF);
                    user_post_powerstate(OFF);
                } else {
                    product_set_switch(ON);
                    user_post_powerstate(ON);
                }
            }
            if ((Mars_get_awss_flag() == 0) && (nCount >= AWSS_REBOOT_CNT)) {
                LOG("do awss reboot");
                do_awss_reboot();
                break;
            } else if ((Mars_get_awss_flag() == 1) && (nCount > AWSS_RESET_CNT)) {
                LOG("do awss reset");
                do_awss_reset();
                break;
            }
            nCount = 0;
        }
        if ((Mars_get_awss_flag() == 0) && (nCount >= AWSS_REBOOT_CNT && awss_mode == 0)) {
            set_net_state(RECONFIGED);
            awss_mode = 1;
        } else if ((Mars_get_awss_flag() == 1) && (nCount > AWSS_RESET_CNT && awss_mode == 0)) {
            set_net_state(UNCONFIGED);
            awss_mode = 1;
        }
        aos_msleep(KEY_DETECT_INTERVAL);
    }
    aos_task_exit(0);
}
#endif

extern void start_netmgr(void *p);
void check_factory_mode(void)
{
    int ret = 0;
    netmgr_ap_config_t ap_config;
    memset(&ap_config, 0, sizeof(netmgr_ap_config_t));

    netmgr_get_ap_config(&ap_config);
    if (strlen(ap_config.ssid) <= 0) {
        LOG("scan factory ap, set led ON");

        ret = scan_factory_ap();
        if (0 != ret) {
            set_net_state(UNCONFIGED);
            LOG("not enter factory mode, start awss");
            Mars_start_wifi_awss(0);
        }
    } else {
        if (Mars_get_awss_flag() == 1) {
            LOG("start awss with netconfig exist");
            set_net_state(RECONFIGED);
            aos_kv_del("awss_flag");
            Mars_start_wifi_awss(Mars_get_awss_flag());
        } else {
            set_net_state(GOT_AP_SSID);
            LOG("start connect");
            aos_task_new("netmgr_start", start_netmgr, NULL, 5120);
        }
    }
}
