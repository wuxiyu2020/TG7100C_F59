/*** 
 * @Description  : 
 * @Author       : zhoubw
 * @Date         : 2022-06-29 15:18:47
 * @LastEditors  : zhoubw
 * @LastEditTime : 2022-07-11 17:18:57
 * @FilePath     : /alios-things/Products/example/mars_template/mars_driver/mars_atcmd.h
 */

#ifndef __MARS_ATCMD_H__
#define __MARS_ATCMD_H__
#ifdef __cplusplus
extern "C" {
#endif


#define ATCMD_START "+++ATCMD\r\n"
#define ATCMD_STARTSUCCESS "AT+CMD=OK\r\n"
#define ATCMD_QUITSUCCESS "AT+QUIT=OK\r\n"

#define ATCMDNULLTEST 			"+ERR=-1\r\n"
#define ATCMDCONNECTTEST 		"+ok\r\n"

bool M_atcmd_getstatus(void);
void M_atcmd_setstatus(bool set_status);
uint32_t M_atcmd_Analyze(char *buf, uint32_t *remainlen);

#ifdef __cplusplus
}
#endif
#endif

