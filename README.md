* [开发环境](#开发环境)
    * [准备开发环境](##准备开发环境)
* [编译方法](#编译方法)
* [alios-things](#alios-things)
    * [目录结构](##目录结构)


# 开发环境
## 准备开发环境
    搭建 SDK 的开发环境。建议您在 64 位 Ubuntu 下搭建设备端 SDK 的开发环境，并使用vim编辑代码。该部分的操作请自行查阅网络相关文档完成。
**说明**
暂不支持在 Windows 系统（含 Windows 子系统）下编译生活物联网平台 SDK。
安装 Ubuntu（版本 16.04 X64）程序运行时库。请您按顺序逐条执行命令。
```
sudo apt-get update
sudo apt-get -y install libssl-dev:i386
sudo apt-get -y install libncurses-dev:i386
sudo apt-get -y install libreadline-dev:i386
```
安装 Ubuntu（版本 16.04 X64）依赖软件包。请您按顺序逐条执行命令。
```
sudo apt-get update
sudo apt-get -y install git wget make flex bison gperf unzip
sudo apt-get -y install gcc-multilib
sudo apt-get -y install libssl-dev
sudo apt-get -y install libncurses-dev
sudo apt-get -y install libreadline-dev
sudo apt-get -y install python python-pip
```
安装 Python 依赖包。请您按顺序逐条执行命令。
```
python -m pip install setuptools
python -m pip install wheel
python -m pip install aos-cube
python -m pip install esptool
python -m pip install pyserial
python -m pip install scons
```
**说明**安装完成后，请您使用 aos-cube --version 查看 aos-cube 的版本号，需确保 aos-cube 的版本号大于等于 0.5.11。
如果在安装过程中遇到网络问题，可使用国内镜像文件。
```
### 安装/升级 pip
python -m pip install --trusted-host=mirrors.aliyun.com -i https://mirrors.aliyun.com/pypi/simple/ --upgrade pip
### 基于 pip 依次安装第三方包和 aos-cube
pip install --trusted-host=mirrors.aliyun.com -i https://mirrors.aliyun.com/pypi/simple/ setuptools
pip install --trusted-host=mirrors.aliyun.com -i https://mirrors.aliyun.com/pypi/simple/ wheel
pip install --trusted-host=mirrors.aliyun.com -i https://mirrors.aliyun.com/pypi/simple/ aos-cube
```

# 编译方法 
```
./build.sh example smart_outlet tg7100cevb SINGAPORE ONLINE 0
```
第一个参数为help时，输出build.sh当前默认编译参数.
第一个参数为clean时，执行SDK目录下example目录删除，并从仓库恢复，下次编译时，会重新完整编译整个SDK及应用.
第一个参数在Products目录下找不到对应的文件夹时，会继续从Living_SDK/example目录下找，如能找到，则执行编译，成功后，复制编译结果到out目录。

要实现不输入参数，执行./build.sh编译输出需要的应用固件，可更改以下默认参数：
```
default_type="example"      //配置产品类型
default_app="smart_outlet"  //配置编译的应用名称
default_board="uno-91h"     //配置编译的模组型号
default_region=SINGAPORE    //配置设备的连云区域,配置为 MAINLAND 或SINGAPORE 都可以，设备可以全球范围内激活
default_env=ONLINE          //配置连云环境，默认设置为线上环境（ONLINE）
default_debug=0             // Debug log 等级
default_args=""             //配置其他编译参数
```
以上参数分别对应：
产品类型、应用名称、模组型号、连云区域、连云环境、debug、其他参数（可不填，需要时把参数加到双引号内）。


当前已支持的board：
tg7100c:    tg7100cevb
RDA5981A: 	hf-lpb130 hf-lpb135 hf-lpt230 hf-lpt130 uno-91h
rtl8710bn: 	mk3080 mk3092
asr5502: 	mx1270


# alios-things

**修改版本号**

```mars_template.mk```文件中修改```CONFIG_FIRMWARE_VERSION```这个宏定义
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