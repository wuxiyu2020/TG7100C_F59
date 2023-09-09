# alios-things

## 目录结构
```
.
├── app_entry.c
├── app_entry.h
├── mars_template_main.c
├── dev_config.h
├── certification
│   ├── ct_cmds.c
│   ├── ct_cmds.h
│   ├── ct_config.h
│   ├── ct_main.c
│   ├── ct_ota.c
│   ├── ct_ota.h
│   ├── ct_simulate.c
│   ├── ct_simulate.h
│   ├── ct_ut.c
│   └── ct_ut.h
├── common
│   ├── combo_net.c
│   ├── combo_net.h
│   ├── device_state_manger.c
│   ├── device_state_manger.h
│   ├── ota_hal_module1.c
│   ├── ota_hal_module2.c
│   ├── property_report.c
│   └── property_report.h
├── mars_driver
│   ├── mars_key.c
│   ├── mars_key.h
│   ├── mars_uart.c
│   └── mars_uart.h
├── mars_driver
│   ├── mars_ota.c
│   ├── mars_ota.h
│   ├── mars_factory.c
│   ├── mars_factory.h
│   ├── mars_network.c
│   ├── mars_network.h
│   ├── mars_uartmsg.c
│   └── mars_uartmsg.h
└── mars_template.json
```
___
## 文件说明
1. app_entry.c&app_entry.h
    ```
    alios 初始化文件，里面包含基本的初始化流程，包括硬件初始化、网络初始化等。
    所做修改:
        1.将硬件初始化函数移动至mars_template_main.c,减少对app_entry的修改
    ```
2. ***mars_template_main.c***
    ```C
    主要逻辑处理功能放在该文件中（应用层逻辑修改文件）：
        1.网络状态回调函数：接收网络状态变更消息，同时做处理;
            /**
             * @brief 网络状态变化事件推送
             * @param {mars_netevent_t} event 网络状态变化事件
             * @return {*}
             */
            void Mars_netevent_callback(mars_netevent_t event); 
        2.网络消息接收回调函数：接收网络消息，并作解析处理;
            /**
             * @brief 网络消息接收回调函数
             * @param {void} *msg 串口解析后的结构体
             * @return {*}
             */
            void Mars_netrecv_callback(void *msg);
        3.串口接收消息回调函数：接收串口消息，并作解析处理;
            /**
             * @brief 串口消息接收回调函数
             * @param {uint8_t} port 串口端口号
             * @param {void} *msg 串口解析后的结构体
             * @return {*}
             */
            void Mars_uartrecv_callback(uint8_t port, void *msg);
    ```
3. ***dev_config.h***
    ```C
    1. 硬件配置头文件，用于配置设备的按键（IO输入）、灯（IO输出）、串口等信息
    2. 设备属性配置相关信息
        ex:
            typedef struct {
                uint8_t powerswitch;
                uint8_t all_powerstate;
            } device_status_t;

            typedef struct _RECV_MSG{
                uint8_t powerswitch;
                uint8_t all_powerstate;
                int flag;
                uint8_t method;
                char seq[24];
                uint8_t from;
            } recv_msg_t;
    3.
    ```
4. mars_driver
    ```
    alios驱动封装文件夹，封装一些基本的驱动，如按键、LED、串口等
    ```
    - mars_uart.c
        ```C
        /**
         * @brief 通用的串口初始化函数，callback函数获取到的为串口接收到的消息
         * @param {void} *uart_set  串口配置结构体，包含引脚、波特率、校验位等信息
         * @param {uart_process_cb} cb 
         * @return {*}
         */
        void Mars_uart_init(void *uart_set, uart_process_cb cb);
        ```

    - mars_key.c
        ```C
        按键初始化等
        ```

5. mars_driver
    ```
    火星人固定流程封装文件夹，针对串口协议的解析、配网离网流程、网络上下行消息的封装放在此文件夹
    ```
    - mars_uartmsg.c
        ```C
        /**
         * @brief 串口初始化函数，指定串口信息和回调函数，callback函数获取到的是解析后的消息结构体指针
         * @param {uint8_t} tx
         * @param {uint8_t} rx
         * @param {mars_uartmsg_recv_cb} cb
         * @return {*}
         */
        void Mars_uartmsg_init(uint8_t tx, uint8_t rx, mars_uartmsg_recv_cb cb);

        /**
         * @brief 串口发送函数（待完善重发机制等）
         * @param {uint8_t} uart_port
         * @param {uint16_t} cmd
         * @param {uint8_t} seq
         * @param {void} *buf
         * @return {*}
         */
        void Mars_uartmsg_send(uint16_t cmd, uint8_t seq, void *buf, uint8_t re_send)
        ```      
    - mars_network.c
        ```C
        /**
         * @brief 网络接口配置函数，进行网络相关功能配置
         * @param {*}
         * @return {*}
         */
        void Mars_network_init(void)
        ```
    - mars_factory.c
        ```C
        产测流程
        ```
    - 

## 注意事项

在阿里云平台添加设备时，**必须按照要求将mac地址作为DeviceName填入**，否则配网过程中APP会提示错误:

![Alt](/Doc/屏幕截图%202022-03-30%20142622.png)

![Alt](/Doc/屏幕截图%202022-03-30%20142149.png)

在设备中执行如下命令
```
linkkey a1n5ZId2YTw 000ea368ca9b 735f33ae2b9cd885da89ce4f4aef907e IJeq2fWLnPVj3inR 10362201
```
![ALT](/Doc/屏幕截图%202022-03-30%20142931.png)