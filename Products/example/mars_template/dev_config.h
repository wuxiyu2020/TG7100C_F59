/*
 *copyright (C) 2019-2022 Alibaba Group Holding Limited
 */

#ifndef __SMART_OUTLET_METER_H__
#define __SMART_OUTLET_METER_H__

#include <aos/aos.h>

//日志上报要用
// #define DEV_MODEL	"E70BC01"
#define DEV_CATEGORY	"JCZ"
#define PRO_NAME        "F59"

#define PK_1            "a1Y7tUwYP8c"
#define PS_1            "nbMKiiroUfxDwQCE"
#define PID_1           "17464922"
#define AD_NAME_1       "F59BC01"

#define PK_2            "a1MrBnZkitD"
#define PS_2            "yIBtNajaLfWaOF9K"
#define PID_2           "17464924"
#define AD_NAME_2       "F59BCZ01"

#define CONFIG_PRINT_HEAP

#define ALIOS_THINGS    (1)

#define MARS_STOVE      (1)
#define MARS_STEAMOVEN  (1)
#define MARS_DISHWASHER (0)
#define MARS_STERILIZER (0)
#define MARS_DISPLAY	(1)

#define EV_UARTS        (EV_USER + 0x0001)
#define EV_NETSTATE     (EV_USER + 0x0002)
#define EV_DEVFUNC      (EV_USER + 0x0100)
#define EV_MODULEOTA	(EV_USER + 0x0101)
#define EV_FACTORY		(EV_USER + 0x0102)
#define EV_TG7100AWSS	(EV_USER + 0x0103)

#define CA_TEMPER_TIME_MS    5000

extern char* get_current_time();
#define mprintf_e(_fmt_, ...)	\
	HAL_Printf("[Mars][%s:%d]Error: "_fmt_"\r\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define mprintf_w(_fmt_, ...)	\
	HAL_Printf("[Mars][%s:%d]Warning: "_fmt_"\r\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define mprintf_d(_fmt_, ...)	\
	HAL_Printf("[%s] "_fmt_"\r\n",  get_current_time(), ##__VA_ARGS__)


#define COMPUTE_BUILD_YEAR \
    ( \
        (__DATE__[ 7] - '0') * 1000 + \
        (__DATE__[ 8] - '0') *  100 + \
        (__DATE__[ 9] - '0') *   10 + \
        (__DATE__[10] - '0') \
    )


#define COMPUTE_BUILD_DAY \
    ( \
        ((__DATE__[4] >= '0') ? (__DATE__[4] - '0') * 10 : 0) + \
        (__DATE__[5] - '0') \
    )


#define BUILD_MONTH_IS_JAN (__DATE__[0] == 'J' && __DATE__[1] == 'a' && __DATE__[2] == 'n')
#define BUILD_MONTH_IS_FEB (__DATE__[0] == 'F')
#define BUILD_MONTH_IS_MAR (__DATE__[0] == 'M' && __DATE__[1] == 'a' && __DATE__[2] == 'r')
#define BUILD_MONTH_IS_APR (__DATE__[0] == 'A' && __DATE__[1] == 'p')
#define BUILD_MONTH_IS_MAY (__DATE__[0] == 'M' && __DATE__[1] == 'a' && __DATE__[2] == 'y')
#define BUILD_MONTH_IS_JUN (__DATE__[0] == 'J' && __DATE__[1] == 'u' && __DATE__[2] == 'n')
#define BUILD_MONTH_IS_JUL (__DATE__[0] == 'J' && __DATE__[1] == 'u' && __DATE__[2] == 'l')
#define BUILD_MONTH_IS_AUG (__DATE__[0] == 'A' && __DATE__[1] == 'u')
#define BUILD_MONTH_IS_SEP (__DATE__[0] == 'S')
#define BUILD_MONTH_IS_OCT (__DATE__[0] == 'O')
#define BUILD_MONTH_IS_NOV (__DATE__[0] == 'N')
#define BUILD_MONTH_IS_DEC (__DATE__[0] == 'D')


#define COMPUTE_BUILD_MONTH \
    ( \
        (BUILD_MONTH_IS_JAN) ?  1 : \
        (BUILD_MONTH_IS_FEB) ?  2 : \
        (BUILD_MONTH_IS_MAR) ?  3 : \
        (BUILD_MONTH_IS_APR) ?  4 : \
        (BUILD_MONTH_IS_MAY) ?  5 : \
        (BUILD_MONTH_IS_JUN) ?  6 : \
        (BUILD_MONTH_IS_JUL) ?  7 : \
        (BUILD_MONTH_IS_AUG) ?  8 : \
        (BUILD_MONTH_IS_SEP) ?  9 : \
        (BUILD_MONTH_IS_OCT) ? 10 : \
        (BUILD_MONTH_IS_NOV) ? 11 : \
        (BUILD_MONTH_IS_DEC) ? 12 : \
        /* error default */  99 \
    )

#define COMPUTE_BUILD_HOUR ((__TIME__[0] - '0') * 10 + __TIME__[1] - '0')
#define COMPUTE_BUILD_MIN  ((__TIME__[3] - '0') * 10 + __TIME__[4] - '0')
#define COMPUTE_BUILD_SEC  ((__TIME__[6] - '0') * 10 + __TIME__[7] - '0')


#define BUILD_DATE_IS_BAD (__DATE__[0] == '?')

#define BUILD_YEAR  ((BUILD_DATE_IS_BAD) ? 99 : COMPUTE_BUILD_YEAR)
#define BUILD_MONTH ((BUILD_DATE_IS_BAD) ? 99 : COMPUTE_BUILD_MONTH)
#define BUILD_DAY   ((BUILD_DATE_IS_BAD) ? 99 : COMPUTE_BUILD_DAY)

#define BUILD_TIME_IS_BAD (__TIME__[0] == '?')

#define BUILD_HOUR  ((BUILD_TIME_IS_BAD) ? 99 :  COMPUTE_BUILD_HOUR)
#define BUILD_MIN   ((BUILD_TIME_IS_BAD) ? 99 :  COMPUTE_BUILD_MIN)
#define BUILD_SEC   ((BUILD_TIME_IS_BAD) ? 99 :  COMPUTE_BUILD_SEC)


void example_free(void *ptr);
void Mars_dev_init(void);

#endif
