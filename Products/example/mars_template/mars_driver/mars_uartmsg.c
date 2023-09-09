/*
 * @Description  : Serial port general protocol message processing.
 * @Author       : zhoubw
 * @Date         : 2022-03-11 15:10:11
 * @LastEditors  : zhoubw
 * @LastEditTime : 2022-11-15 15:35:19
 * @FilePath     : /tg7100c/Products/example/mars_template/mars_driver/mars_uartmsg.c
 */


#include <stdio.h>
#include <string.h>
// #include <stdlib.h>
// #include <stdarg.h>

#include "../dev_config.h"

#if ALIOS_THINGS
#include <aos/aos.h>
#include <hal/soc/uart.h>
#else
#endif
#include "k_api.h"

#include "mars_uartmsg.h"
#include "mars_atcmd.h"
#include "mars_httpc.h"
#include "mars_cloud.h"

// #include "netmgr.h"
// #include "iot_export.h"
// #include "iot_import.h"
// #include "dev_config.h"
#include "../mars_devfunc/mars_devmgr.h"
// #include "vendor.h"

#define ECB_RESEND_SUPPORT

static mars_uartmsg_recv_cb g_mars_uartmsg_cmd_cb = NULL;
static uart_dev_t mars_uart = {0};

static uint16_t g_uartmsg_ackseq = 0xFFFF;
typedef enum{
    UARTLOG_RECV,
    UARTLOG_SEND
};

#if ALIOS_THINGS

#define GET_SDK_DATAWIDTH(x)    x==M_DATAWIDTH_8BIT?DATA_WIDTH_8BIT: \
                                (x==M_DATAWIDTH_7BIT?DATA_WIDTH_7BIT: \
                                (x==M_DATAWIDTH_6BIT?DATA_WIDTH_6BIT: \
                                (x==M_DATAWIDTH_5BIT?DATA_WIDTH_5BIT:DATA_WIDTH_8BIT)))

#define GET_SDK_STOPBITS(x)     x==M_STOPBITS_1?STOP_BITS_1: \
                                (x==M_STOPBITS_2?STOP_BITS_2:STOP_BITS_1)

#define GET_SDK_FLOWCONTROL(x)  x==M_FLOWCONTROL_DISABLE?FLOW_CONTROL_DISABLED: \
                                (x==M_FLOWCONTROL_CTS?FLOW_CONTROL_CTS: \
                                (x==M_FLOWCONTROL_RTS?FLOW_CONTROL_RTS: \
                                (x==M_FLOWCONTROL_CTS_RTS?FLOW_CONTROL_CTS_RTS:FLOW_CONTROL_DISABLED)))

#define GET_SDK_PARITY(x)       x==M_NO_PARITY?NO_PARITY:   \
                                (x==M_ODD_PARITY?ODD_PARITY: \
                                (x==M_EVEN_PARITY?EVEN_PARITY:NO_PARITY))
#else

#endif


#define MSG_SEND_LOOP           (20)
static dlist_t g_uartmsg_send_list;
// static AOS_DLIST_HEAD(g_uartmsg_send_list);
// static void *g_msglist_mutex = NULL;
static aos_mutex_t g_msglist_mutex;
static aos_task_t task_mars_uartmsg_send;
static aos_task_t task_mars_uartmsg_recv;

void mars_uartmsg_ackseq(uint16_t ack_seq)
{
    g_uartmsg_ackseq = ack_seq;
}

void mars_uartmsg_ackseq_by_seq(uint16_t ack_seq)
{
    dlist_t* p = NULL;
    dlist_t* n = NULL;
    mars_msglist_t* tmp = NULL;

    if (!dlist_empty(&g_uartmsg_send_list))
    {
        aos_mutex_lock(&g_msglist_mutex, AOS_WAIT_FOREVER);
        dlist_for_each_safe(p, n, &g_uartmsg_send_list)
        {
            tmp = list_entry(p, mars_msglist_t, list_node);
            if (tmp->u16_seq == ack_seq)
            {
                //LOGW("mars", "串口通讯: [序列号=%d 命令字=%d] 收到应答,本次收发耗时%d毫秒", tmp->u16_seq, (tmp->msg)[4], aos_now_ms()-tmp->send_time);
                dlist_del(&tmp->list_node);
                LOGW("mars", "本次删除完成, 发送队列元素个数 = %d (删除原因: 收到应答)", dlist_entry_number(&g_uartmsg_send_list));
                aos_free(tmp->msg);
                aos_free(tmp);
            }
        }
        aos_mutex_unlock(&g_msglist_mutex);
    }
}

void output_hex_string(char* head, uint8_t* buf, uint16_t buf_len)
{
    int len = (buf_len * 2 + 1);
    char* log_buf = (char *)aos_malloc(len);
    if (log_buf)
    {
        memset(log_buf, 0, len);
        for (int a=0; a<buf_len; ++a)
        {
            char tmp[4] = {0};
            sprintf(tmp, "%02X", buf[a]);
            strcat(log_buf, tmp);
        }
        LOGI("mars", "%s[%d] %s", head, buf_len, log_buf);
        aos_free(log_buf);
    }
}

void output_hex_string_space(char* head, uint8_t* buf, uint16_t buf_len)
{
    int len = (buf_len * 3 + 1);
    char* log_buf = (char *)aos_malloc(len);
    if (log_buf)
    {
        memset(log_buf, 0, len);
        for (int a=0; a<buf_len; ++a)
        {
            char tmp[4] = {0};
            sprintf(tmp, "%02X ", buf[a]);
            strcat(log_buf, tmp);
        }
        LOGI("mars", "%s[%d] %s", head, buf_len, log_buf);
        aos_free(log_buf);
    }
}

char* get_cmd_type(uint8_t cmd)
{
    if (cmd == cmd_get)
        return "发送查询";
    else if (cmd == cmd_set)
        return "发送设置";
    else if (cmd == cmd_store)
        return "发送store";
    else if (cmd == cmd_ota_0A)
        return "发送ota";
    else if (cmd == cmd_reqstatusack)
        return "应答reqstatus";
     else if (cmd == cmd_ack)
        return "应答确认";
    else if (cmd == cmd_nak)
        return "应答否认";
    else
        return "不明报文";
}

void mars_uartmsg_send_thread(void *argv)
{
    dlist_t* p = NULL;
    dlist_t* n = NULL;
    mars_msglist_t* tmp = NULL;
    uint64_t time_last = 0;
    while(true)
    {
        if ((aos_now_ms() - time_last >= 500) && !dlist_empty(&g_uartmsg_send_list))
        {         
            aos_mutex_lock(&g_msglist_mutex, AOS_WAIT_FOREVER);
            dlist_for_each_safe(p, n, &g_uartmsg_send_list)
            {
                tmp = list_entry(p, mars_msglist_t, list_node);
                output_hex_string("串口通讯 通讯板--->电控板: ", tmp->msg, tmp->len);
                Mars_uart_send(mars_uart.port, tmp->msg, tmp->len);
                tmp->send_time = aos_now_ms();
                time_last = tmp->send_time;
                tmp->send_cnt--;
                if (tmp->send_cnt == 3)
                    LOGI("mars", "串口通讯: [序列号=%d 命令字=%d 类型=%s] 首次发送",  tmp->u16_seq, (tmp->msg)[4], get_cmd_type((tmp->msg)[4]));
                else if (tmp->send_cnt == 2)
                    LOGW("mars", "串口通讯 [序列号=%d 命令字=%d 类型=%s] 第%d次重发", tmp->u16_seq, (tmp->msg)[4], get_cmd_type((tmp->msg)[4]), (3-tmp->send_cnt));
                else if (tmp->send_cnt == 1)
                    LOGE("mars", "串口通讯 [序列号=%d 命令字=%d 类型=%s] 第%d次重发", tmp->u16_seq, (tmp->msg)[4], get_cmd_type((tmp->msg)[4]), (3-tmp->send_cnt));
                else if (tmp->send_cnt == 0)
                    LOGE("mars", "串口通讯 [序列号=%d 命令字=%d 类型=%s] 第%d次重发", tmp->u16_seq, (tmp->msg)[4], get_cmd_type((tmp->msg)[4]), (3-tmp->send_cnt));

                if (tmp->send_cnt == 0)
                {
                    //LOGE("mars", "串口通讯: [序列号=%d 命令字=%d 类型=%s] 3次重发机会已用完!!!", tmp->u16_seq, (tmp->msg)[4], get_cmd_type((tmp->msg)[4]));
                    dlist_del(&tmp->list_node);
                    LOGW("mars", "本次删除完成, 发送队列元素个数 = %d (删除原因: 重发超时)", dlist_entry_number(&g_uartmsg_send_list));
                    aos_free(tmp->msg);
                    aos_free(tmp);
                }

                break;
            }
            aos_mutex_unlock(&g_msglist_mutex);
        }

        aos_msleep(50);
    }

    aos_task_exit(0);
}

static void M_uartmsg_addlist(void *msg_buf, uint16_t msg_len, uint16_t msg_seq, uint8_t re_send, uint16_t waitTime_ms)
{
    if (msg_buf != NULL && msg_len > 0)
    {
        mars_msglist_t *msg_node = (mars_msglist_t *)aos_malloc(sizeof(mars_msglist_t));
        //LOGI("mars", "M_uartmsg_addlist: aos_malloc = 0x%08X", msg_node);
        if(msg_node)
        {
            memset(msg_node, 0, sizeof(mars_msglist_t));
            dlist_init(&msg_node->list_node);

            msg_node->msg       = msg_buf;
            msg_node->len       = msg_len;
            msg_node->u16_seq   = msg_seq;
            msg_node->send_cnt  = 4;            // (re_send == 0?1:re_send); 初次发送1 + 重发3次
            msg_node->delay_cnt = waitTime_ms;  // 本次发送的超时时间 (可以精确控制每一个报文的超时时间: 例如ota的应答，时间需要较长)
            msg_node->send_time = 0;            // 上次发送时间

            aos_mutex_lock(&g_msglist_mutex, AOS_WAIT_FOREVER);
            dlist_add_tail(&msg_node->list_node, &g_uartmsg_send_list);
            LOGW("mars", "本次添加完成, 发送队列元素个数 = %d", dlist_entry_number(&g_uartmsg_send_list));
            aos_mutex_unlock(&g_msglist_mutex);
        }
    }  
}

#define QUE_ENABLE 1
#if QUE_ENABLE
static aos_task_t task_mars_uartmsg_process;
#define UARTMSG_SUM         5

static aos_queue_t *g_uartmsg_queue_id = NULL;
static char *g_uartmsg_queue_buff = NULL;
static void mars_uartmsg_queue_init(void)
{
    g_uartmsg_queue_id = (aos_queue_t *) aos_malloc(sizeof(aos_queue_t));
    g_uartmsg_queue_buff = aos_malloc(UARTMSG_SUM * sizeof(uartmsg_que_t));
    aos_queue_new(g_uartmsg_queue_id, g_uartmsg_queue_buff, UARTMSG_SUM * sizeof(uartmsg_que_t), sizeof(uartmsg_que_t));
}

void mars_uartmsg_process_thread(void *argv)
{
    unsigned int recv_len = 0;
    uartmsg_que_t msg = {0};
    uint64_t time_mem = 0;
    uint64_t time_get = 0;
    uint64_t time_weather = 0;
    bool ota_ing = false;

    while (true) 
    {
        if (aos_queue_recv(g_uartmsg_queue_id, AOS_NO_WAIT, &msg, &recv_len) == 0) //AOS_WAIT_FOREVER 传入0获取不到会立即报失败
        {
            if (g_mars_uartmsg_cmd_cb)
            {
                g_mars_uartmsg_cmd_cb(1, &msg);
            }
        }
        
        /*
        说明: 如果频繁调用api函数krhino_overview()的话，会造成如下崩溃
        === backtrace_trap start ===
        -f -p 23049bf4  23049bea 2301112e 23048a3e
        backtrace_trap: reached task deathbed!!!
        =Boot Reason:        
        BL_RST_SOFTWARE_WATCHDOG
        */
        if ((time_mem == 0) || (aos_now_ms() - time_mem) >= (10*1000))
        {
            extern void print_heap();
            print_heap();
            //krhino_overview();  //不能频繁调用
            time_mem = aos_now_ms();

            if (time_mem > 0 && !ota_ing)
            {
                extern void ota_boot_manual(void);
                //ota_boot_manual();
                ota_ing = true;
            }
        }

        if (time_get == 0 || (aos_now_ms() - time_get) >= (60*1000))
        {
            mars_devmngr_getstatus(NULL, NULL);
            time_get = aos_now_ms();
        }

        aos_msleep(50);
    }

    aos_task_exit(0);
}
#endif

uint16_t uart_get_seq_mid(void)
{
    static uint16_t seq = 0;
    return seq++;
}

static void InvertUint16(unsigned short *dBuf, unsigned short *srcBuf)
{
    int i;
    unsigned short tmp[4] = {0};

    for (i = 0; i < 16; i++)
    {
        if (srcBuf[0] & (1 << i))
            tmp[0] |= 1 << (15 - i);
    }
    dBuf[0] = tmp[0];
}

unsigned short CRC16_MAXIM(const unsigned char *data, unsigned int datalen)
{
    unsigned short wCRCin = 0x0000;
    unsigned short wCPoly = 0x8005;

    InvertUint16(&wCPoly, &wCPoly);
    while (datalen--)
    {
        wCRCin ^= *(data++);
        for (int i = 0; i < 8; i++)
        {
            if (wCRCin & 0x01) {
                wCRCin = (wCRCin >> 1) ^ wCPoly;
            } else {
                wCRCin = wCRCin >> 1;
            }
        }
    }

    // LOG("CRC16_MAXIM result (0x%04x).", (wCRCin ^ 0xFFFF));
    return (wCRCin ^ 0xFFFF);
}

//单包数据的CRC16校验算法 (CRC-16/MAXIM x16+x15+x2+1)
uint16_t crc16_maxim_single(unsigned char *ptr, int len)
{
    unsigned int i;
    unsigned short crc = 0x0000;
     
    while(len--)
    {
        crc ^= *ptr++;
        for (i = 0; i < 8; ++i)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc = (crc >> 1);
        }
    }
     
    return ~crc;
}

//多包数据的CRC16校验算法 (CRC-16/MAXIM x16+x15+x2+1)
#if 0
uint16_t crc16_maxim_multi(unsigned char *ptr, int len, uint16_t before_crc)
{
    unsigned int i;
    unsigned short crc = before_crc;
     
    while(len--)
    {
        crc ^= *ptr++;
        for (i = 0; i < 8; ++i)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc = (crc >> 1);
        }
    }
     
    return ~crc;
}
#endif
/**
 * @brief 这里用来解析火星人固定协议的串口消息
 * @param {uint8_t} port
 * @param {uint32_t} len
 * @param {void} *buf
 * @return {*}
 */
extern void clear_recv_buff(void);
static void mars_uart_process_cb(uint8_t port, void *buf, uint32_t len, uint32_t *remain_len)
{
    uint8_t *u8_buf = (uint8_t *)buf;
    uartmsg_que_t recv_msg = {0};
    uint32_t msg_head = 0;
    *remain_len = len > UART_BUF_SIZE?UART_BUF_SIZE:len;

    if (M_atcmd_getstatus() && ((*remain_len) > 3))
    {
        LOGW("mars", "AT接收[%d]:  %s ", *remain_len, (char *)u8_buf);
    }

    while(*remain_len)
    {
        if (true == M_atcmd_getstatus())
        {
            char *atcmd_start = NULL;
            atcmd_start = strstr((char *)u8_buf, ATCMD_START);
            if (atcmd_start)
            {
                M_atcmd_setstatus(true);
                Mars_uart_send(1, ATCMD_STARTSUCCESS, strlen(ATCMD_STARTSUCCESS));
                *remain_len = 0;
                clear_recv_buff();
                break;
            }
            msg_head = M_atcmd_Analyze((char *)u8_buf, remain_len);
            *remain_len = 0;
            clear_recv_buff();
        }
        else
        {
            char *atcmd_start = NULL;
            atcmd_start = strstr((char *)u8_buf, ATCMD_START);
            if (atcmd_start)
            {
                M_atcmd_setstatus(true);
                Mars_uart_send(1, ATCMD_STARTSUCCESS, strlen(ATCMD_STARTSUCCESS));
                msg_head = 0;
                // memset(u8_buf, 0, *remain_len);
                *remain_len = 0;
                clear_recv_buff();
                // memcpy(u8_buf, atcmd_start+strlen(ATCMD_START), *remain_len - (atcmd_start-u8_buf+strlen(ATCMD_START)));
                break;
            }

            //寻找包头
            int i=0;
            for(i=msg_head; i < len-1; ++i)
            {
                if ((u8_buf[i] == 0xE6) && (u8_buf[i+1] == 0xE6))
                {
                    msg_head = i;
                    break;
                }
            }

            if (msg_head >= len -1)
            {
                *remain_len = 0;
                break;
            }

            if(11 <= *remain_len){   //长度满足最小包
                memset(&recv_msg, 0, sizeof(uartmsg_que_t));
                recv_msg.port = port;
                recv_msg.seq = ((uint16_t)u8_buf[msg_head+2] << 8) | (uint16_t)u8_buf[msg_head+3];
                recv_msg.cmd = u8_buf[msg_head+4];
                recv_msg.len = ((uint16_t)u8_buf[msg_head+5] << 8) | (uint16_t)u8_buf[msg_head+6];
                if (recv_msg.len <= (*remain_len-11) && (recv_msg.len <= 1024))
                {        
                    recv_msg.check_sum = ((uint16_t)u8_buf[msg_head+7+recv_msg.len] << 8) | (uint16_t)u8_buf[msg_head+8+recv_msg.len];
                    uint32_t chk_len = recv_msg.len + 5;
                    if (recv_msg.check_sum == CRC16_MAXIM(u8_buf + msg_head + 2, chk_len) && (u8_buf[msg_head+9+recv_msg.len] == 0x6E && u8_buf[msg_head+10+recv_msg.len] == 0x6E))
                    {
                        if (recv_msg.len > 0)
                        {
                            recv_msg.msg_buf = (uint8_t *)aos_malloc(sizeof(uint8_t)*(recv_msg.len));
                            memcpy(recv_msg.msg_buf, u8_buf+msg_head+7 , recv_msg.len);
                        }                            

                        char temp_str[128] = {0x00};
                        snprintf(temp_str, sizeof(temp_str), "串口通讯 wifi <--- ecb(腰部板 seq=%d): ", recv_msg.seq);
                        output_hex_string(temp_str, u8_buf + msg_head, recv_msg.len + 11);

                        if (cmd_getack == recv_msg.cmd || cmd_event == recv_msg.cmd)
                        {                                    
                            char *log_buf = (char *)aos_malloc((recv_msg.len+11)*2+40);
                            if (log_buf)
                            {
                                memset(log_buf, 0, ((recv_msg.len+11)*2+40));
                                sprintf(log_buf, M_CLOUD_LOG_UARTHEAD, (uint32_t)(mars_getTimestamp_ms()/1000));
                                for (int a=0;a<(recv_msg.len+11);++a)
                                {
                                    char tmp[4] = {0};
                                    sprintf(tmp, "%02X", u8_buf[msg_head+a]);
                                    strcat(log_buf, tmp);
                                }
                                //strcat(log_buf, M_CLOUD_LOG_UARTTAIL);
                                LOGI("mars", "串口解析: 收到电控板状态帧");
                                mars_cloud_logpost(log_buf, strlen(log_buf));
                                aos_free(log_buf);
                            }
                        }

#if QUE_ENABLE
                            int err = aos_queue_send(g_uartmsg_queue_id, &recv_msg, sizeof(uartmsg_que_t));
                            if (0 != err)
                            {
                                if (recv_msg.msg_buf)
                                {
                                    aos_free(recv_msg.msg_buf);
                                    recv_msg.msg_buf = NULL;
                                }
                            }
#else
                            if (g_mars_uartmsg_cmd_cb)
                            {
                                g_mars_uartmsg_cmd_cb(recv_msg.port, &recv_msg);
                            }
#endif
                        *remain_len = *remain_len-11-recv_msg.len;
                        msg_head = msg_head + 11 + recv_msg.len;
                    }
                    else
                    {
                        *remain_len = *remain_len-1;
                        msg_head = msg_head + 1;
                    }
                }else{
                    //*remain_len = *remain_len-1;
                    //msg_head = msg_head + 1;
                    *remain_len = *remain_len;
                    break;
                }
            }else{
                //找到包头，长度不满足最小包
                *remain_len = *remain_len;
                break;
            }
        }
        //process end
    }

    if (*remain_len != 0){
        //printf("over: 1. *remain_len=%d\r\n", *remain_len);
        memcpy(u8_buf, u8_buf+msg_head, *remain_len);
        msg_head = 0;
    }else{
        //printf("over: 2\r\n");
        bzero(u8_buf, sizeof(u8_buf));
    }

    if (*remain_len > 256)
    {
        LOGI("mars", "接收缓冲区清零!!!!!!(%d)" , *remain_len);
        *remain_len = 0;
    }

    //printf("结束: len = %d\r\n", *remain_len);
    //output_hex_string_space("结束: dat=", u8_buf, *remain_len);
}

/**
 * @brief 
 * @param {uint8_t} uart_port
 * @param {uint16_t} cmd
 * @param {uint8_t} seq
 * @param {void} *buf
 * @return {*}
 */
void Mars_uartmsg_send(uint16_t cmd, uint16_t seq, void *buf, uint16_t buf_len, uint8_t re_send)
{
    if (M_atcmd_getstatus())  //AT模式
    {
        return;
    }

    extern bool is_module_ota();
    if (is_module_ota())    //OTA模式
    {
        if (cmd != cmd_ota_0A)
        {
            return;
        }
    }

    int32_t err = 0;
    uint8_t *send_msg = (uint8_t *)aos_malloc(UARTMSGT_MIN + buf_len + 1);  
    uint16_t msg_len = 0;  
    if (send_msg)
    {        
        send_msg[msg_len++] = 0xe6;
        send_msg[msg_len++] = 0xe6;
        send_msg[msg_len++] = (uint8_t)(seq >> 8);
        send_msg[msg_len++] = (uint8_t)(seq & 0xFF);
        send_msg[msg_len++] = cmd;
        send_msg[msg_len++] = (uint8_t)(buf_len >> 8);
        send_msg[msg_len++] = (uint8_t)(buf_len & 0xFF);
        if(buf != NULL && buf_len > 0)
        {
            memcpy(send_msg+msg_len, buf, buf_len);
            msg_len += buf_len;
        }        

        uint16_t temp_crc = CRC16_MAXIM(send_msg+2, msg_len-2);
        send_msg[msg_len++] = (uint8_t)(temp_crc >> 8);
        send_msg[msg_len++] = (uint8_t)(temp_crc & 0xFF);
        send_msg[msg_len++] = 0x6e;
        send_msg[msg_len++] = 0x6e;

        if (cmd_set == cmd || cmd_store == cmd || cmd_get == cmd /*|| cmd_ota == cmd*/)
        {            
            char* log_buf = (char *)aos_malloc(msg_len*2+40);            
            if (log_buf != NULL)
            {
                memset(log_buf, 0, (msg_len*2+40));
                sprintf(log_buf, M_CLOUD_LOG_UARTHEAD, (uint32_t)(mars_getTimestamp_ms()/1000));
                for (int a=0; a<msg_len; ++a)
                {
                    char tmp[4] = {0};
                    sprintf(tmp, "%02X", send_msg[a]);
                    strcat(log_buf, tmp);
                }
                //strcat(log_buf, M_CLOUD_LOG_UARTTAIL);
                mars_cloud_logpost(log_buf, strlen(log_buf));
                aos_free(log_buf);
                log_buf = NULL;
            }
        }  

        if (re_send > 0)    
        {
            M_uartmsg_addlist(send_msg, msg_len, seq, re_send, 500);            
        }
        else
        {
            Mars_uart_send(mars_uart.port, send_msg, msg_len);
            output_hex_string("串口通讯 wifi ---> ecb: ", send_msg, msg_len);\
            aos_free(send_msg);
        }
    }
    else
    {
        LOGE("mars", "串口通讯 wifi ---> ecb: 内存分配失败!!! (长度=%d)", UARTMSGT_MIN + buf_len + 1);
    }

    return err;
}

void Mars_uartmsg_init( uint8_t port,
                        uint8_t tx,
                        uint8_t rx,
                        uint32_t baud_rate,
                        mars_uart_datawidth_t data_width,
                        mars_uart_stopbits_t stop_bits,
                        mars_uart_flowcontrol_t flow_conrtol,
                        mars_uart_parity_t parity, 
                        mars_uartmsg_recv_cb cb)
{
    LOGI("mars", "电控板通讯串口: 串口=uart%d 波特率=%d 发送=pin%d 接收=pin%d", port, baud_rate, tx, rx);
#if ALIOS_THINGS
    uint8_t uart_pins[3] = {0};
    uart_pins[0] = tx;
    uart_pins[1] = rx;

    mars_uart.port = port;
    mars_uart.config.baud_rate = baud_rate;
    mars_uart.config.data_width = GET_SDK_DATAWIDTH(data_width);
    mars_uart.config.flow_control = GET_SDK_FLOWCONTROL(flow_conrtol);
    mars_uart.config.stop_bits = GET_SDK_STOPBITS(stop_bits);
    mars_uart.config.parity = GET_SDK_PARITY(parity);
 // mars_uart.config.mode = MODE_TX_RX;
    mars_uart.priv = uart_pins;
#else
    mars_uart_config_t mars_uart = {0};
#endif

    if(cb){
        g_mars_uartmsg_cmd_cb = cb;
    }

    dlist_init(&g_uartmsg_send_list);
    if (0 != aos_mutex_new(&g_msglist_mutex)) 
    {
        LOGE("mars", "aos_mutex_new: failed!!!");
        return;
    }

    Mars_uart_init(&mars_uart, mars_uart_process_cb);

    int ret = aos_task_new_ext(&task_mars_uartmsg_send, "mars_uartmsg_send_thread", mars_uartmsg_send_thread, NULL, 4*1024, AOS_DEFAULT_APP_PRI + 3);
    if (ret != 0)
        LOGE("mars", "aos_task_new_ext: failed!!! (串口发送线程)");

    extern void mars_uartmsg_recv_thread(void *argv);
    ret = aos_task_new_ext(&task_mars_uartmsg_recv, "mars_uartmsg_recv_thread", mars_uartmsg_recv_thread, NULL, 4*1024, AOS_DEFAULT_APP_PRI + 3);
    if (ret != 0)
        LOGE("mars", "aos_task_new_ext: failed!!! (串口处理:数据接收线程)");

#if QUE_ENABLE
    //说明:串口解析到一个完整数据后,是发送给数据处理线程进行处理呢？还是直接在串口接收函数里面处理呢?
    mars_uartmsg_queue_init();
    ret = aos_task_new_ext(&task_mars_uartmsg_process, "mars_uartmsg_process_thread", mars_uartmsg_process_thread, NULL, 6*1024, AOS_DEFAULT_APP_PRI);
    if (ret != 0)
        LOGE("mars", "aos_task_new_ext: failed!!! (串口处理:数据处理线程)");
#endif
}

static uint8_t g_uart_rec_buf[UART_BUF_SIZE+1] = {0};
static uint32_t remain_len = 0;
static uart_dev_t dev_uart[UART_SUM] = {0};
static uart_process_cb g_mars_uart_process_cb[UART_SUM] = {NULL};

void clear_recv_buff(void)
{
    memset(g_uart_rec_buf, 0x00, sizeof(g_uart_rec_buf));
}

void mars_uartmsg_recv_thread(void *argv)
{
    uint32_t rx_size = 0;
    while(true)
    {
        hal_uart_recv_II(&dev_uart[1], g_uart_rec_buf+remain_len , UART_BUF_SIZE-remain_len, &rx_size, 20);
        if (rx_size > 0)
        {
            //output_hex_string("[串口接收]: ", g_uart_rec_buf+remain_len, rx_size);
            if (g_mars_uart_process_cb[1])
            {
                g_mars_uart_process_cb[1](1, g_uart_rec_buf, remain_len+rx_size, &remain_len);
            }
        }

        aos_msleep(50);
    }

    aos_task_exit(0);
}

/**
 * @brief 
 * @param {uint8_t} uart_port
 * @param {void} *buf
 * @param {uint32_t} len
 * @return {*}
 */
int32_t Mars_uart_send(uint8_t uart_port, const void *buf, uint32_t len)
{
    return hal_uart_send_poll(&dev_uart[uart_port], buf, (uint32_t)len);
}

/**
 * @brief 
 * @param {uart_dev_t} *uart_info
 * @param {uart_process_cb} cb
 * @return {*}
 */
int32_t Mars_uart_init(void *uart_set, uart_process_cb cb)
{
    int32_t err = 0;
    uart_dev_t *uart_info = (uart_dev_t *)uart_set;    


    // dev_uart[uart_info->port] = uart_info;
    memcpy(&dev_uart[uart_info->port], uart_info, sizeof(uart_dev_t)); 

    if (NULL != cb) {
        g_mars_uart_process_cb[uart_info->port] = cb;
    }

    //注册串口消息回调(串口底层有数据会自动通知)
    //hal_uart_recv_cb_reg(&dev_uart[uart_info->port], g_mars_uart_recv_cb);

    err = hal_uart_init(&dev_uart[uart_info->port]);
    return err;
}

void Mars_uart_deinit(uint8_t port)
{
    return;
}
