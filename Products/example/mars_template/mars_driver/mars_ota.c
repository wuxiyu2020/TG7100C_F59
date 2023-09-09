/*
 * @Description  : 
 * @Author       : zhoubw
 * @Date         : 2022-06-06 13:38:47
 * @LastEditors  : zhoubw
 * @LastEditTime : 2022-11-29 09:22:12
 * @FilePath     : /tg7100c/Products/example/mars_template/mars_driver/mars_ota.c
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <aos/aos.h>

#include "../dev_config.h"
#include "../mars_devfunc/mars_devmgr.h"
#include "ota_service.h"
#include "iot_export.h"
#include "mars_ota.h"


static int mars_ota_init(void);
static ota_service_t ctx = {0};
static bool ota_service_inited = false;

void mars_ota_status(int status)
{
    ctx.upg_status = status;
    if(ctx.trans_protcol != OTA_PROTCOL_COAP_LOCAL) {
        ctx.h_tr->status(0, &ctx);
    }
}

bool mars_ota_inited(void)
{
    return ota_service_inited;
}

#ifdef CERTIFICATION_TEST_MODE
void *ct_entry_get_uota_ctx(void)
{
    if (ota_service_inited == true)
    {
        return (void *)&ctx;
    }
    else
    {
        return NULL;
    }
}
#endif

void mars_dev_OTANeedReboot(void)
{
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();
    if (mars_template_ctx->status.OTAbyAPP || mars_template_ctx->status.SysPower == 0)
    {
        LOGI("mars", "ota: wifi板重启1......(立即重启)\r\n");
        mars_template_ctx->ota_reboot = 0;
        ota_reboot();
    }
    else
    {
        mars_template_ctx->ota_reboot = 1;
        LOGI("mars", "ota: wifi板重启2......(延迟重启)\r\n");
    }
}

void mars_ota_reboot(void)
{
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();
    if (1 == mars_template_ctx->ota_reboot)
    {
        LOGI("mars", "ota: wifi板重启......\r\n");
        mars_template_ctx->ota_reboot = 0;
        ota_reboot();
    }
}

// int mars_ota_event_callback(enum MARS_OTA_EVENT_ID event_id, void *param)
// {
// 	switch(event_id)
// 	{
//         case MARS_EVENT_OTAREBOOT_SET:
//         {
//             break;
//         }
//         case MARS_EVENT_OTAMODULE_STEP:
//         {
//             extern void moduleota_step(void);
//             aos_post_delayed_action(300, moduleota_step, NULL);
//             break;
//         }
//         default:
//             break;
// 	}

// 	return 0;
// }

static int mars_ota_init(void)
{
#if defined(OTA_ENABLED) && defined(BUILD_AOS)
    // aos_register_event_filter(EV_MODULEOTA, mars_ota_event_callback, NULL);
    char product_key[PRODUCT_KEY_LEN + 1] = {0};
    char device_name[DEVICE_NAME_LEN + 1] = {0};
    char device_secret[DEVICE_SECRET_LEN + 1] = {0};
    HAL_GetProductKey(product_key);
    HAL_GetDeviceName(device_name);
    HAL_GetDeviceSecret(device_secret);
    memset(&ctx, 0, sizeof(ota_service_t));
    strncpy(ctx.pk, product_key, sizeof(ctx.pk)-1);
    strncpy(ctx.dn, device_name, sizeof(ctx.dn)-1);
    strncpy(ctx.ds, device_secret, sizeof(ctx.ds)-1);
    ctx.trans_protcol = 0;
    ctx.dl_protcol = 3;
    ota_service_init(&ctx);
#endif
    return 0;
}

void mars_ota_module_1_init(void)
{
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();
    extern ota_hal_module_t mars_ota_module_1;
    iotx_ota_module_info_t module_1 = {0};

    if (mars_template_ctx->status.ElcSWVersion == 0x00)
    {
        return;
    }

    memcpy(module_1.module_name, "dis", strlen("dis"));
    sprintf(module_1.module_version, "%X.%X",  (mars_template_ctx->status.ElcSWVersion>>4), (mars_template_ctx->status.ElcSWVersion & 0x0F));
    memcpy(module_1.product_key, ctx.pk, sizeof(ctx.pk)-1);
    memcpy(module_1.device_name, ctx.dn, sizeof(ctx.dn)-1);
    module_1.hal = &mars_ota_module_1;
    ota_service_set_module_info(&ctx, &module_1);
}

void mars_ota_module_2_init(void)
{
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();
    extern ota_hal_module_t mars_ota_module_2;
    iotx_ota_module_info_t module_2 = {0};

    if (mars_template_ctx->status.PwrSWVersion == 0x00)
    {
        return;
    }

    memcpy(module_2.module_name, "pwr", strlen("pwr"));
    sprintf(module_2.module_version, "%X.%X", \
            (mars_template_ctx->status.PwrSWVersion>>4), \
            (mars_template_ctx->status.PwrSWVersion & 0x0F));
    // memcpy(module_2.module_version, "0.0.1", strlen("0.0.1"));
    memcpy(module_2.product_key, ctx.pk, sizeof(ctx.pk)-1);
    memcpy(module_2.device_name, ctx.dn, sizeof(ctx.dn)-1);
    module_2.hal = &mars_ota_module_2;
    ota_service_set_module_info(&ctx, &module_2);
}

int mars_ota_mqtt_connected(void)
{
    if (ota_service_inited) 
    {
        int ret = 0;

        LOG("MQTT reconnected, let's redo OTA upgrade");
        if ((ctx.h_tr) && (ctx.h_tr->upgrade)) 
        {
            LOG("Redoing OTA upgrade");
            ret = ctx.h_tr->upgrade(&ctx);
            if (ret < 0) LOG("Failed to do OTA upgrade");
        }

        return ret;
    }    

#ifdef DEV_OFFLINE_OTA_ENABLE
    ota_service_inform(&ctx);
#else
    mars_ota_init();
#endif

    mars_ota_module_1_init();
    mars_ota_module_2_init();
    // extern void mars_get_ota_para();
    // mars_get_ota_para();
    ota_service_inited = true;
    LOGW("mars", "模块ota: ota初始化完成");

    return 0;
}

