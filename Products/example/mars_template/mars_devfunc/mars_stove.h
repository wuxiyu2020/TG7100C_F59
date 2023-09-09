/*** 
 * @Description  : 
 * @Author       : zhoubw
 * @Date         : 2022-08-17 16:58:09
 * @LastEditors  : zhoubw
 * @LastEditTime : 2022-11-11 09:55:54
 * @FilePath     : /tg7100c/Products/example/mars_template/mars_devfunc/mars_stove.h
 */
#ifndef __MARS_STOVE_H__
#define __MARS_STOVE_H__
#ifdef __cplusplus
extern "C" {
#endif

#include "cJSON.h"
#include "mars_devmgr.h"

#if MARS_STOVE
                                
/*** 
 * @brief 灶具串口消息上报处理函数
 * @param {uartmsg_que_t} *msg
 * @param {mars_template_ctx_t} *mars_template_ctx
 * @param {uint16_t} *index
 * @param {bool} *report_en
 * @param {uint8_t} *nak
 * @return {*}
 */
void mars_stove_uartMsgFromSlave(uartmsg_que_t *msg, \
                                mars_template_ctx_t *mars_template_ctx, \ 
                                uint16_t *index, bool *report_en, uint8_t *nak);

/*** 
 * @brief 灶具云端下发命令处理函数
 * @param {cJSON} *root
 * @param {cJSON} *item
 * @param {mars_template_ctx_t} *mars_template_ctx
 * @param {uint8_t} *buf_setmsg
 * @param {uint16_t} *buf_len
 * @return {*}
 */
void mars_stove_setToSlave(cJSON *root, cJSON *item, \
                            mars_template_ctx_t *mars_template_ctx, \
                            uint8_t *buf_setmsg, uint16_t *buf_len);


/*** 
 * @brief 灶具属性状态上报
 * @param {cJSON} *proot
 * @param {mars_template_ctx_t} *mars_template_ctx
 * @return {*}
 */
void mars_stove_changeReport(cJSON *proot, mars_template_ctx_t *mars_template_ctx);



#endif

#ifdef __cplusplus
}
#endif
#endif