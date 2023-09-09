/*** 
 * @Description  : 
 * @Author       : zhoubw
 * @Date         : 2022-08-17 17:00:44
 * @LastEditors  : zhoubw
 * @LastEditTime : 2022-10-29 15:11:37
 * @FilePath     : /alios-things/Products/example/mars_template/mars_devfunc/mars_dishwasher.h
 */
#ifndef __MARS_DISHWASHER_H__
#define __MARS_DISHWASHER_H__
#ifdef __cplusplus
extern "C" {
#endif

#include "cJSON.h"
#include "mars_devmgr.h"

#if MARS_DISHWASHER


/*** 
 * @brief 洗碗机串口消息上报处理函数
 * @param {uartmsg_que_t} *msg
 * @param {mars_template_ctx_t} *mars_template_ctx
 * @param {uint16_t} *index
 * @param {bool} *report_en
 * @param {uint8_t} *nak
 * @return {*}
 */
void mars_dishwasher_uartMsgFromSlave(uartmsg_que_t *msg, 
                                mars_template_ctx_t *mars_template_ctx, 
                                uint16_t *index, bool *report_en, uint8_t *nak);

/*** 
 * @brief 洗碗机云端下发控制处理函数
 * @param {cJSON} *root
 * @param {cJSON} *item
 * @param {mars_template_ctx_t} *mars_template_ctx
 * @param {uint8_t} *buf_setmsg
 * @param {uint16_t} *buf_len
 * @return {*}
 */
void mars_dishwasher_setToSlave(cJSON *root, cJSON *item, \
                            mars_template_ctx_t *mars_template_ctx, \
                            uint8_t *buf_setmsg, uint16_t *buf_len);

/*** 
 * @brief 洗碗机属性改变上报
 * @param {cJSON} *proot
 * @param {mars_template_ctx_t} *mars_template_ctx
 * @return {*}
 */
void mars_dishwasher_changeReport(cJSON *proot, mars_template_ctx_t *mars_template_ctx);

#endif

#ifdef __cplusplus
}
#endif
#endif
