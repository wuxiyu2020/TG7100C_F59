/*** 
 * @Description  : 
 * @Author       : zhoubw
 * @Date         : 2022-09-02 13:41:48
 * @LastEditors  : zhoubw
 * @LastEditTime : 2022-10-29 15:34:26
 * @FilePath     : /alios-things/Products/example/mars_template/mars_devfunc/mars_display.h
 */

#ifndef __MARS_HEADER_H__
#define __MARS_HEADER_H__
#ifdef __cplusplus
extern "C" {
#endif

#include "cJSON.h"
#include "mars_devmgr.h"

#if MARS_DISPLAY


void get_weather(void *arg1, void *arg2);
void get_weather_timer_start(void);


/*** 
 * @brief 头部显示板串口消息处理
 * @param {uartmsg_que_t} *msg
 * @param {mars_template_ctx_t} *mars_template_ctx
 * @param {uint16_t} *index
 * @param {bool} *report_en
 * @param {uint8_t} *nak
 * @return {*}
 */
void mars_display_uartMsgFromSlave(uartmsg_que_t *msg, \
                                mars_template_ctx_t *mars_template_ctx, \
                                uint16_t *index, bool *report_en, uint8_t *nak);


/*** 
 * @brief 头部显示板云端命令下发处理
 * @param {cJSON} *root
 * @param {cJSON} *item
 * @param {mars_template_ctx_t} *mars_template_ctx
 * @param {uint8_t} *buf_setmsg
 * @param {uint16_t} *buf_len
 * @return {*}
 */
bool mars_display_setToSlave(cJSON *root, cJSON *item, \
                            mars_template_ctx_t *mars_template_ctx, \
                            uint8_t *buf_setmsg, uint16_t *buf_len);


/*** 
 * @brief 头部显示板属性变更上报
 * @param {cJSON} *proot
 * @param {mars_template_ctx_t} *mars_template_ctx
 * @return {*}
 */
void mars_display_changeReport(cJSON *proot, mars_template_ctx_t *mars_template_ctx);


#endif

#ifdef __cplusplus
}
#endif
#endif

