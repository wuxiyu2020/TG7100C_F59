/*** 
 * @Description  : 
 * @Author       : zhoubw
 * @Date         : 2022-08-17 17:01:22
 * @LastEditors  : zhoubw
 * @LastEditTime : 2022-10-29 15:41:47
 * @FilePath     : /alios-things/Products/example/mars_template/mars_devfunc/mars_sterilizer.h
 */
#ifndef __MARS_STERILIZER_H__
#define __MARS_STERILIZER_H__
#ifdef __cplusplus
extern "C" {
#endif

#if MARS_STERILIZER

#include "cJSON.h"
#include "mars_devmgr.h"



/*** 
 * @brief 消毒柜
 * @param {uartmsg_que_t} *msg
 * @param {mars_template_ctx_t} *mars_template_ctx
 * @param {uint16_t} *index
 * @param {bool} *report_en
 * @param {uint8_t} *nak
 * @return {*}
 */
void mars_sterilizer_uartMsgFromSlave(uartmsg_que_t *msg, 
                                mars_template_ctx_t *mars_template_ctx, 
                                uint16_t *index, bool *report_en, uint8_t *nak);

#endif

#ifdef __cplusplus
}
#endif
#endif