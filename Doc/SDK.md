<!--
 * @Description  : 
 * @Author       : zhoubw
 * @Date         : 2022-04-15 09:28:20
 * @LastEditors  : zhoubw
 * @LastEditTime : 2022-10-10 10:22:48
 * @FilePath     : /alios-things/Doc/SDK.md
-->

Path:alios-things/Living_SDK/platform/mcu/tg7100c/hal_drv/tg7100c_hal


## 开启蓝牙广播

bt_le_adv_start

ble_advertising_start

breeze_start_advertising | breeze_restart_advertising

combo_restart_ble_adv

combo_service_evt_handler

breeze_awss_init

breeze_start

transport_rx

trans_rx_dispatcher

extcmd_rx_command

auth_rx_command

m_apinfo_handler

ext_cmd06_rsp

m_tlv_type_handler_table

收到WiFi信息后

combo_service_evt_handler

wifi_connect_handler

## AP扫描
Living_SDK/framework/protocol/linkkit/sdk/iotx-sdk-c_clone/src/services/awss/awss_aplist.c
Living_SDK/framework/protocol/linkkit/sdk/iotx-sdk-c_clone/include/exports/iot_export_diagnosis.h

awss_save_apinfo：
dump_awss_status(STATE_WIFI_CHAN_SCAN, "[%d] ssid:%s, mac:%02x%02x%02x%02x%02x%02x, chn:%d, rssi:%d, adha:%d",
               i, ssid, bssid[0], bssid[1], bssid[2],
               bssid[3], bssid[4], bssid[5], channel,
               rssi > 0 ? rssi - 256 : rssi, adha);

通过ssid获取AP信息
zconfig_get_apinfo_by_ssid

awss_save_apinfo (IOT_RegisterCallback(ITE_STATE_EVERYTHING, dev_diagnosis_statecode_handler))(dev_diagnosis_statecode_handler)

awss_ieee80211_aplist_process

awss_protocol_couple_array

ieee80211_data_extract

zconfig_recv_callback

aws_80211_frame_handler

aws_start

## flash
prebuild/include/hal/soc/flash.h
Living_SDK/platform/mcu/tg7100c/hal/hal_flash_tg7100c.c

## OTA
ota_boot
ota_get_version
bl_fw_ota_stop

Living_SDK/framework/uOTA/ota_service.c   ota_download_thread  hal_boot2_update_ptable ota_upgrade_cb 

PtTable_Update_Entry

Products/example/mars_template/app_entry.c      ota_service_init(&ctx)
                                                ota_service_inform
                                                ctx->h_tr = ota_get_transport()
                                                ota_trans_inform

## LOG
MQTT_COMM_ENABLED
IOT_MQTT_LogPost

## version
iotx_report_firmware_version

## list
utils_list_init()


# http

iot_export_http2.h

IOT_HTTP_Init
IOT_HTTP_SendMessage


http用旧的方案


# md5
    utils_md5_init(&context);                                      /* init context for 1st pass */
    utils_md5_starts(&context);                                    /* setup context for 1st pass */
    utils_md5_update(&context, k_ipad, KEY_IOPAD_SIZE);            /* start with inner pad */
    utils_md5_update(&context, (unsigned char *) msg, msg_len);    /* then text of datagram */
    utils_md5_finish(&context, out); 

aos_post_delayed_action



debug_fatal_error