/*** 
 * @Author: zhoubw
 * @Date: 2022-03-11 11:41:04
 * @LastEditors  : zhoubw
 * @LastEditTime : 2022-09-19 17:08:28
 * @FilePath     : /alios-things/Products/example/mars_template/mars_devfunc/mars_factory.h
 * @Description: 
 */

#ifndef __MARS_FACTORY_H__
#define __MARS_FACTORY_H__
#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mars_devmgr.h"

void mars_factory_uartMsgFromSlave(uint8_t cmd, mars_template_ctx_t *mars_template_ctx);

uint8_t mars_factory_status(void);
void mars_factory_wifiTest(void);
void mars_factory_awss(void);
void mars_factory_likkey(void);
#ifdef __cplusplus
}
#endif
#endif
