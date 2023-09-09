/*** 
 * @Description  : 
 * @Author       : zhoubw
 * @Date         : 2022-06-06 13:39:09
 * @LastEditors  : zhoubw
 * @LastEditTime : 2022-11-04 15:22:58
 * @FilePath     : /tg7100c/Products/example/mars_template/mars_driver/mars_ota.h
 */

#ifndef __MARS_OTA_H__
#define __MARD_OTA_H__
#ifdef __cplusplus
extern "C" {
#endif

enum MARS_OTA_ERR{
    MARS_OTAERR_NONE,
};

enum MARS_OTA_EVENT_ID{
    MARS_EVENT_OTAREBOOT_SET,
    MARS_EVENT_OTAMODULE_STEP,
};

bool mars_ota_inited(void);
int mars_ota_mqtt_connected(void);

#ifdef __cplusplus
}
#endif
#endif

