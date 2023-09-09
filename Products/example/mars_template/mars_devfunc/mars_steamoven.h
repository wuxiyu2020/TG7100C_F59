/*** 
 * @Description  : 
 * @Author       : zhoubw
 * @Date         : 2022-08-17 16:58:44
 * @LastEditors  : zhoubw
 * @LastEditTime : 2022-10-29 15:38:15
 * @FilePath     : /alios-things/Products/example/mars_template/mars_devfunc/mars_steamoven.h
 */
#ifndef __MARS_STEAMOVEN_H__
#define __MARS_STEAMOVEN_H__
#ifdef __cplusplus
extern "C" {
#endif

#include "cJSON.h"
#include "mars_devmgr.h"

#if MARS_STEAMOVEN

/*** 
 * @brief 蒸烤箱串口消息处理函数
 * @param {uartmsg_que_t} *msg
 * @param {mars_template_ctx_t} *mars_template_ctx
 * @param {uint16_t} *index
 * @param {bool} *report_en
 * @param {uint8_t} *nak
 * @return {*}
 */
void mars_steamoven_uartMsgFromSlave(uartmsg_que_t *msg, \
                                mars_template_ctx_t *mars_template_ctx, \
                                uint16_t *index, bool *report_en, uint8_t *nak);

/*** 
 * @brief 蒸烤箱云端命令下发处理函数
 * @param {cJSON} *root
 * @param {cJSON} *item
 * @param {mars_template_ctx_t} *mars_template_ctx
 * @param {uint8_t} *buf_setmsg
 * @param {uint16_t} *buf_len
 * @return {*}
 */
void mars_steamoven_setToSlave(cJSON *root, cJSON *item, \
                            mars_template_ctx_t *mars_template_ctx, \
                            uint8_t *buf_setmsg, uint16_t *buf_len);


/*** 
 * @brief 蒸烤箱属性变更上报
 * @param {cJSON} *proot
 * @param {mars_template_ctx_t} *mars_template_ctx
 * @return {*}
 */
void mars_steamoven_changeReport(cJSON *proot, mars_template_ctx_t *mars_template_ctx);

#endif

#ifdef __cplusplus
}
#endif
#endif