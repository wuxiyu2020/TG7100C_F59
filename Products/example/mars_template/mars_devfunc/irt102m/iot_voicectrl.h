#ifndef __XM_VOICECTRL_H__
#define __XM_VOICECTRL_H__
#include <aos/aos.h>


#define XM_VOICECTRL
//#define XM_VOICECTRL_EXAMPLE
typedef int (*xm_voice_callback_t)( char * /*data*/,int /*len*/);


#define VOICE_DATA_SIZE 128  
#define VOICE_UART_TAIL 0x55

#define XM_VOICE_SUCCESS 1
#define XM_VOICE_FAIL    0


typedef enum {
  GPIO_NUM_A25 = 0,   ///<可用于SPI-MISO / ADC / DMIC-DATA
  GPIO_NUM_A26,       ///<可用于SPI-CLK / ADC / DMIC-CLK
  GPIO_NUM_A27,       ///<可用于SPI-MOSI / PWM / ADC
  GPIO_NUM_A28,       ///<已占用，PA使能控制（喇叭功放静音）
  GPIO_NUM_B0,        ///<已占用，SW-CLK（烧录器接口）
  GPIO_NUM_B1,        ///<已占用，SW-DATA（烧录器接口）
  GPIO_NUM_B2,        ///<可用于UART1-TX / PWM / I2C-SCL
  GPIO_NUM_B3,        ///<可用于UART1-RX / PWM / I2C-SDA
  GPIO_NUM_B6,        ///<UART1-RX（外设串口通信接收脚），不使能UART时可用做GPIO
  GPIO_NUM_B7,        ///<UART1-TX（外设串口通信发送脚），不使能UART是可用做GPIO
  GPIO_NUM_B8,        ///<已占用，虚拟Software UART-TX（Log输出引脚，波特率115200）
  GPIO_NUM_MAX
}GPIO_NUMBER;

typedef enum {
  XM_GPIO_OUTPUT,
  XM_GPIO_INPUT,
  XM_GPIO_INTER,
  OPEN_VOICE,
  SET_VOLUME,
  GET_VERSION
}GPIO_TYPE;

typedef enum {
  COMMUNICATION_UART,
  COMMUNICATION_I2C	
}COMMIT_TYPE;

typedef enum {
  SET_VOLUME_MIN,    //设置最小音量
  SET_VOLUME_MID,    //设置中等音量
  SET_VOLUME_MAX,    //设置最大音量
  SET_VOLUME_UP,     //增加音量
  SET_VOLUME_DOWN,   //减小音量
  SET_PLAYER_STOP,   //停止播放
  SET_SPAKER_MUTE,    // 设置喇叭静音
  SET_SPAKER_UNMUTE,    //取消设置喇叭静音
  SET_PLAYER_SHUTUP_MODE,  //系统进入无回复播报模式
  SET_PLAYER_SHUTUP_EXIT   //系统退出无回复播报模式
}VOLUME_TYPE;

/**
 * @brief voicectrl uart data.
 *
 * @param[in] data: uart recv data
 *			 len: the length of data, in bytes
 * @return[out] the data length of send success, -1 is send fail
 * @see None.
 * @note None
 */
int xm_voicectrl_uart(unsigned char *data,int len);
/**
 * @brief voicectrl register.
 *
 * @param[in] xm_voice_callback_t: callback
 * @return[out] the data length of send success, -1 is send fail
 * @see None.
 * @note None
 */
int xm_voicectrl_register(xm_voice_callback_t xm_callback);
/**
 * @brief control A05S gpio.
 *
 * @param[in] GPIO_NUMBER: gpio io
 *                   level :gpio level
 * @return[out] the data length of send success, -1 is send fail
 * @see None.
 * @note None
 */

int xm_gpio_output(GPIO_NUMBER gpio,int level);
/**
 * @brief get A05S communication mode.
 *
 * @param[in] None
 * @return[COMMIT_TYPE]  type of communication
 * @see None.
 * @note None
 */
int xm_voice_get_communication_mode(void);

/**
 * @brief set A05S communication mode.
 *
 * @param[in] COMMIT_TYPE:type ofcommunication
 * @return[]  None
 * @see None.
 * @note None
 */
void xm_voice_set_communication_mode(COMMIT_TYPE type);
/**
 * @brief set A05S volume.
 *
 * @param[in] VOLUME_TYPE:type of volume
 * @return[]  None
 * @see None.
 * @note None
 */

int xm_voice_set_volume(VOLUME_TYPE type);
/**
 * @brief open A05S voice file.
 *
 * @param[in] file: the file of mp3 name
 * @return[]  None
 * @see None.
 * @note None
 */
int xm_open_voice_file(char *file);
/**
 * @brief get A05S version.
 *
 * @param[in] None
 * @return[]  the string of version
 * @see None.
 * @note None
 */
unsigned char* xm_voice_get_voice_version(void);


int xm_i2c_master_recv(uint8_t *data,int len);
int xm_i2c_master_send(uint8_t *data,int len);
int32_t aos_uart2_send(uint8_t *data, uint32_t size, uint32_t timeout);

int32_t aos_uart2_recv(unsigned char *buf, int len, uint32_t timeout);


#endif
