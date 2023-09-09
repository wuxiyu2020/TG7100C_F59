/*** 
 * @Description  : Serial port general protocol message processing.
 * @Author       : zhoubw
 * @Date         : 2022-03-11 15:13:31
 * @LastEditors  : zhoubw
 * @LastEditTime : 2022-11-09 13:55:51
 * @FilePath     : /tg7100c/Products/example/mars_template/mars_driver/mars_uartmsg.h
 */

#ifndef __MARS_UARTMSG_H__
#define __MARS_UARTMSG_H__
#ifdef __cplusplus
extern "C" {
#endif

#include <aos/aos.h>
#include "../dev_config.h"

#define UARTMSG_BUFSIZE    (1024)
#define UARTMSGT_MIN       (11)

typedef enum{
   M_DATAWIDTH_5BIT,
   M_DATAWIDTH_6BIT,
   M_DATAWIDTH_7BIT,
   M_DATAWIDTH_8BIT,
}mars_uart_datawidth_t;

typedef enum{
   M_STOPBITS_1,
   M_STOPBITS_2,
}mars_uart_stopbits_t;

typedef enum{
   M_FLOWCONTROL_DISABLE,     /* Flow control disabled */
   M_FLOWCONTROL_CTS,         /* Clear to send, yet to send data */
   M_FLOWCONTROL_RTS,         /* Require to send, yet to receive data */
   M_FLOWCONTROL_CTS_RTS,     /* Both CTS and RTS flow control */
}mars_uart_flowcontrol_t;

typedef enum{
   M_NO_PARITY,      /* No parity check */
   M_ODD_PARITY,     /* Odd parity check */
   M_EVEN_PARITY,    /* Even parity check */
}mars_uart_parity_t;

typedef enum _MSG_FROM{
    FROM_PROPERTY_SET = 0,
    FROM_SERVICE_SET
} msg_from_t;

#pragma pack(1)

typedef struct{
      uint8_t                 uart_port;
      uint8_t                 tx;
      uint8_t                 rx;
      uint32_t                baud_rate;
      mars_uart_datawidth_t   data_width;
      mars_uart_parity_t      parity;
      mars_uart_stopbits_t    stop_bits;
      mars_uart_flowcontrol_t flow_control;
}mars_uart_config_t;

/**
 * | 固定帧头 | 时序号 | 命令码 | 数据长度 | 数据内容 | 校验码  | 固定帧尾 |
 * |  E6E6   | 2Bytes | 1Byte | 2Byte   | 1141Bytes| 2Bytes |  6E6E   |
 */
typedef struct {
   uint8_t  port;
   uint16_t seq;
   uint8_t  cmd;
   uint16_t len;
   uint8_t  *msg_buf;
   uint16_t check_sum;
}uartmsg_que_t;

typedef struct mars_msglist_s {
   dlist_t list_node;
   uint16_t u16_seq;
   uint16_t len;
   uint8_t *msg;
   uint8_t  send_cnt;
   uint16_t delay_cnt;
   uint64_t send_time;
}mars_msglist_t;
#pragma pack()

void mars_uartmsg_ackseq(uint16_t ack_seq);
void mars_uartmsg_ackseq_by_seq(uint16_t ack_seq);
uint16_t uart_get_seq_mid(void);
void Mars_uartmsg_send(uint16_t cmd, uint16_t seq, void *buf, uint16_t buf_len, uint8_t re_send);

/*** 
 * @brief 串口消息接收、处理的回调函数。为防止消息阻塞，建议处理后的消息放入事件中处理 
 * @param {uint8_t} port 串口端口号
 * @param {void} *msg 接收串口消息解析后的结构体
 * @return {*}
 */
typedef int (*mars_uartmsg_recv_cb)(uint8_t port, void *msg);

/*** 
 * @brief 
 * @param {uart_port} port
 * @param {uint8_t} tx
 * @param {uint8_t} rx
 * @param {uint32_t} baud_rate
 * @param {mars_uart_datawidth_t} data_width
 * @param {mars_uart_stopbits_t} stop_bits
 * @param {mars_uart_flowcontrol_t} flow_conrtol
 * @param {mars_uart_parity_t} parity
 * @param {mars_uartmsg_recv_cb} cb
 * @return {*}
 */
void Mars_uartmsg_init( uint8_t port, 
                        uint8_t tx, 
                        uint8_t rx,
                        uint32_t baud_rate,
                        mars_uart_datawidth_t data_width,
                        mars_uart_stopbits_t stop_bits,
                        mars_uart_flowcontrol_t flow_conrtol,
                        mars_uart_parity_t parity, 
                        mars_uartmsg_recv_cb cb);

#define UART_SUM        2
#define UART_BUF_SIZE   1024 

#define UART0_PORT  (0)
#define UART1_PORT  (1)
// typedef enum{
//     UART0_PORT  = 0,
//     UART1_PORT  = 1,
// }uart_port;

typedef void (*uart_process_cb)(uint8_t port, void *buf, uint32_t len, uint32_t *buf_remain);

/**
 * @brief 
 * @param {uint8_t} uart_port
 * @param {void} *buf
 * @param {uint32_t} len
 * @return {*}
 */
int32_t Mars_uart_send(uint8_t uart_port, const void *buf, uint32_t len);

int32_t Mars_uart_init(void *uart_info, uart_process_cb cb);
uint16_t crc16_maxim_single(unsigned char *ptr, int len);
//uint16_t crc16_maxim_multi (unsigned char *ptr, int len, uint16_t before_crc);

                        
#ifdef __cplusplus
}
#endif
#endif
