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
#include "app_entry.h"
#include "aos/kv.h"
#include <hal/soc/gpio.h>
#include <hal/soc/uart.h>
#include "vendor.h"
#include "common/device_state_manger.h"
#include "mars_driver/mars_network.h"
#include "iot_import_awss.h"
#include "dev_config.h"
#if defined(HF_LPT230) || defined(HF_LPT130)
#include "hfilop/hfilop.h"
#endif

#include "mars_devfunc/mars_devmgr.h"
#if  (defined (TG7100CEVB))
char *p_product_key = NULL;
char *p_product_secret = NULL;
char *p_device_name = NULL;
char *p_device_secret = NULL;
char *productidStr = NULL;
#else /* default config  */
#define LED_GPIO    22
#define RELAY_GPIO  5
#define KEY_GPIO    4
#endif


#if 0
#if (REBOOT_STATE == LAST_STATE)
aos_timer_t g_timer_period_save_device_status;
#define PERIOD_SAVE_DEVICE_STATUS_INTERVAL (1000 * 30)
static void timer_period_save_device_status_cb(void *arg1, void *arg2)
{
}
#endif


void vendor_product_init(void)
{
    #if 0

    io_led.port = LED_GPIO;
    io_relay.port = RELAY_GPIO;
    io_key.port = KEY_GPIO;

    io_led.config = OUTPUT_PUSH_PULL;
    io_relay.config = OUTPUT_PUSH_PULL;
    io_key.config = INPUT_PULL_UP;

    hal_gpio_init(&io_led);
    hal_gpio_init(&io_relay);
    hal_gpio_init(&io_key);

    product_init_switch();

    #endif
#if (REBOOT_STATE == LAST_STATE)
    aos_timer_new(&g_timer_period_save_device_status, timer_period_save_device_status_cb, NULL, PERIOD_SAVE_DEVICE_STATUS_INTERVAL, 1);
#endif
}
#endif

int vendor_get_product_key(char *product_key, int *len)
{
    char *pk = NULL;
    int ret = -1;
    int pk_len = *len;

    ret = aos_kv_get("linkkit_product_key", product_key, &pk_len);
#if defined(HF_LPT230) || defined(HF_LPT130)
    if ((ret != 0)&&((pk = hfilop_layer_get_product_key()) != NULL)) {
        pk_len = strlen(pk);
        memcpy(product_key, pk, pk_len);
        ret = 0;
    }
#else
    /*
        todo: get pk...
    */
#endif
    if (ret == 0) {
        *len = pk_len;
    }
#if (defined (TG7100CEVB))
    if(p_product_key != NULL && strlen(p_product_key) > 0){
        pk_len = strlen(p_product_key);
        memcpy(product_key, p_product_key, pk_len);
        *len = pk_len;
        ret = 0;
    }
#endif
    return ret;
}

int vendor_get_product_secret(char *product_secret, int *len)
{
    char *ps = NULL;
    int ret = -1;
    int ps_len = *len;

    ret = aos_kv_get("linkkit_product_secret", product_secret, &ps_len);
#if defined(HF_LPT230) || defined(HF_LPT130)
    if ((ret != 0)&&((ps = hfilop_layer_get_product_secret()) != NULL)) {
        ps_len = strlen(ps);
        memcpy(product_secret, ps, ps_len);
        ret = 0;
    }
#else
    /*
        todo: get ps...
    */
#endif
    if (ret == 0) {
        *len = ps_len;
    }
#if (defined (TG7100CEVB))
    if(p_product_secret != NULL && strlen(p_product_secret) > 0){
        ps_len = strlen(p_product_secret);
        memcpy(product_secret, p_product_secret, ps_len);
        *len = ps_len;
        ret = 0;
    }
#endif
    return ret;
}

int vendor_get_device_name(char *device_name, int *len)
{
    char *dn = NULL;
    int ret = -1;
    int dn_len = *len;

    ret = aos_kv_get("linkkit_device_name", device_name, &dn_len);
#if defined(HF_LPT230) || defined(HF_LPT130)
    if ((ret != 0)&&((dn = hfilop_layer_get_device_name()) != NULL)) {
        dn_len = strlen(dn);
        memcpy(device_name, dn, dn_len);
        ret = 0;
    }
#else
    /*
        todo: get dn...
    */
#endif
    if (ret == 0) {
        *len = dn_len;
    }
#if (defined (TG7100CEVB))
    if(p_device_name != NULL && strlen(p_device_name) > 0){
        dn_len = strlen(p_device_name);
        memcpy(device_name, p_device_name, dn_len);
        *len = dn_len;
        ret = 0;
    }
#endif
    return ret;
}

int vendor_get_device_secret(char *device_secret, int *len)
{
    char *ds = NULL;
    int ret = -1;
    int ds_len = *len;

    ret = aos_kv_get("linkkit_device_secret", device_secret, &ds_len);
#if defined(HF_LPT230) || defined(HF_LPT130)
    if ((ret != 0)&&((ds = hfilop_layer_get_device_secret()) != NULL)) {
        ds_len = strlen(ds);
        memcpy(device_secret, ds, ds_len);
        ret = 0;
    }
#else
    /*
        todo: get ds...
    */
#endif
    if (ret == 0) {
        *len = ds_len;
    }
#if (defined (TG7100CEVB))
    if(p_device_secret != NULL && strlen(p_device_secret) > 0){
        ds_len = strlen(p_device_secret);
        memcpy(device_secret, p_device_secret, ds_len);
        *len = ds_len;
        ret = 0;
    }
#endif
    return ret;
}

int vendor_get_product_id(uint32_t *pid)
{
    int ret = -1;
    char pidStr[11] = { 0 };
    int len = sizeof(pidStr);

    ret = aos_kv_get("linkkit_product_id", pidStr, &len);
    if (ret == 0 && len < sizeof(pidStr)) {
        *pid = atoi(pidStr);
    } else {
        ret = -1;
    }
#if (defined (TG7100CEVB))
    if(productidStr != NULL && strlen(productidStr) > 6){
        *pid = atoi(productidStr);
        // LOG("pid[%d]", *pid);
        return 0;
    }
#endif
    return ret;
}

int set_device_meta_info(void)
{
    int len = 0;
    char product_key[PRODUCT_KEY_LEN + 1]       = {0};
    char product_secret[PRODUCT_SECRET_LEN + 1] = {0};
    char device_name[DEVICE_NAME_LEN + 1]       = {0};
    char device_secret[DEVICE_SECRET_LEN + 1]   = {0};
    char pidStr[11]                             = { 0 };

    memset(product_key,    0, sizeof(product_key));
    memset(product_secret, 0, sizeof(product_secret));
    memset(device_name,    0, sizeof(device_name));
    memset(device_secret,  0, sizeof(device_secret));
    memset(pidStr,         0, sizeof(pidStr));

    len = PRODUCT_KEY_LEN + 1;
    vendor_get_product_key(product_key, &len);
    len = PRODUCT_SECRET_LEN + 1;
    vendor_get_product_secret(product_secret, &len);
    len = DEVICE_NAME_LEN + 1;
    vendor_get_device_name(device_name, &len);
    len = DEVICE_SECRET_LEN + 1;
    vendor_get_device_secret(device_secret, &len);
    len = sizeof(pidStr);
    aos_kv_get("linkkit_product_id", pidStr, &len);

    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();
    if ( (strlen(product_key)    > 0) && 
         (strlen(product_secret) > 0) && 
         (strlen(device_name)    > 0) && 
         (strlen(device_secret)  > 0)) 
    {    
        memcpy(mars_template_ctx->product_key,    product_key,    sizeof(product_key));
        memcpy(mars_template_ctx->product_secret, product_secret, sizeof(product_secret));
        memcpy(mars_template_ctx->device_name,    device_name,    sizeof(device_name));
        memcpy(mars_template_ctx->device_secret,  device_secret,  sizeof(device_secret));

        HAL_SetProductKey(product_key);
        HAL_SetProductSecret(product_secret);
        HAL_SetDeviceName(device_name);
        HAL_SetDeviceSecret(device_secret);
        LOGI("mars", "--------------四元组信息--------------");
        LOGI("mars", "四元组  pk=%s",  product_key);
        LOGI("mars", "四元组  dn=%s",  device_name);
        LOGI("mars", "四元组  ds=%s",  device_secret);
        LOGI("mars", "四元组  ps=%s",  product_secret);
        LOGI("mars", "四元组 pid=%s",  pidStr);
        return 0;
    } else {
#if (defined (TG7100CEVB))
        /* check media valid, and update p */
	extern int ali_factory_media_get(char **pp_product_key, char **pp_product_secret,
        			char **pp_device_name, char **pp_device_secret, char **pp_pidStr);
        int res = ali_factory_media_get(
                    &p_product_key,
                    &p_product_secret,
                    &p_device_name,
                    &p_device_secret,
                    &productidStr);
        if (0 != res) {
            printf("ali_factory_media_get res = %d\r\n", res);
            return -1;
        } else {
            memcpy(mars_template_ctx->product_key, product_key, sizeof(product_key));
            memcpy(mars_template_ctx->product_secret, product_secret, sizeof(product_secret));
            memcpy(mars_template_ctx->device_name, device_name, sizeof(device_name));
            memcpy(mars_template_ctx->device_secret, device_secret, sizeof(device_secret));

            HAL_SetProductKey(p_product_key);
            HAL_SetProductSecret(p_product_secret);
            HAL_SetDeviceName(p_device_name);
            HAL_SetDeviceSecret(p_device_secret);
            LOG("pk[%s]", p_product_key);
            LOG("dn[%s]", p_device_name);
            // LOG("pid[%s]", productidStr);
            return 0;
        }
      
#endif
        LOGI("mars", "设备不存在四元组信息!!!!!");
        return -1;
    }



}

void linkkit_reset(void *p);
void vendor_device_bind(void)
{
    set_net_state(APP_BIND_SUCCESS);
}

void vendor_device_unbind(void)
{
    linkkit_reset(NULL);
}
void vendor_device_reset(void)
{
    /* do factory reset */
    // clean kv ...
    // clean buffer ...
    /* end */
    do_awss_reset();
}
