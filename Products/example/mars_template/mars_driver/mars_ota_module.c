#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "aos/kernel.h"
#include "aos/kv.h"
#include "hal/soc/flash.h"
#include "ota_hal_plat.h"
#include "ota_hal_os.h"
#include "iot_import.h"

#include "mars_uartmsg.h"
#include "../mars_devfunc/mars_devmgr.h"

#define TMP_BUF_LEN 1024
#define OTA_RESEND  3

static unsigned int _offset = 0;
static uint16_t ota_seq = 0;
static bool g_moduleota_taskstatus = false;
static char* ota_target_desc[] = {"显示板", "电源板", "头部版"};
static bool request_flag = false;

void delay_reset(void *p)
{
    LOGW("mars", "ota延迟重启开始计时......(5秒后复位)");    
    aos_msleep(5000);
    LOGW("mars", "ota开始复位");
    HAL_Reboot();
    aos_task_exit(0);
}

static aos_task_t task_delay_reset;
void do_delay_reset(void)
{
    aos_task_new_ext(&task_delay_reset, "delay reset task", delay_reset, NULL, 1024, 0);
}

/*将大写字母转换成小写字母*/  
int tolower(int c)  
{  
    if (c >= 'A' && c <= 'Z')  
    {  
        return c + 'a' - 'A';  
    }  
    else  
    {  
        return c;  
    }  
}

//将十六进制的字符串转换成整数  
int htoi(char s[])  
{  
    int i;  
    int n = 0;  
    if (s[0] == '0' && (s[1]=='x' || s[1]=='X'))  
    {  
        i = 2;  
    }  
    else  
    {  
        i = 0;  
    }  
    for (; (s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'z') || (s[i] >='A' && s[i] <= 'Z');++i)  
    {  
        if (tolower(s[i]) > '9')  
        {  
            n = 16 * n + (10 + tolower(s[i]) - 'a');  
        }  
        else  
        {  
            n = 16 * n + (tolower(s[i]) - '0');  
        }  
    }  
    return n;  
}

static int ota_image_check(uint32_t image_size)  //火星人的多模块固件尾部并不包含16字节的MD5值
{
    uint32_t filelen, flashaddr, len = 0, left;
    uint8_t md5_recv[16];
    uint8_t md5_calc[16];
    ota_md5_context ctx;
    uint8_t *tmpbuf;

    tmpbuf = (uint8_t *)aos_malloc(TMP_BUF_LEN);

    filelen = image_size - 16;
    flashaddr = filelen;
    hal_flash_read(HAL_PARTITION_OTA_TEMP, &flashaddr, (uint8_t *)md5_recv, 16);

    ota_md5_init(&ctx);
    ota_md5_starts(&ctx);
    flashaddr = 0;
    left = filelen;

    while (left > 0)
    {
        if (left > TMP_BUF_LEN)
        {
            len = TMP_BUF_LEN;
        }
        else
        {
            len = left;
        }
        left -= len;
        hal_flash_read(HAL_PARTITION_OTA_TEMP, &flashaddr, (uint8_t *)tmpbuf, len);
        ota_md5_update(&ctx, (uint8_t *)tmpbuf, len);
    }

    ota_md5_finish(&ctx, md5_calc);
    ota_md5_free(&ctx);

    aos_free(tmpbuf);
    if (memcmp(md5_calc, md5_recv, 16) != 0)
    {
        LOGE("mars", "md5校验失败!!!!!! error");
        return -1;
    }

    LOGI("mars", "md5校验成功");
    return 0;
}

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
    return crc;         //这里不取反
}

static int ota_image_crc(uint32_t image_size)
{
    uint16_t crcout = 0x0000;
    uint32_t filelen, flashaddr, len = 0, left;
    uint8_t *tmpbuf = (uint8_t *)aos_malloc(TMP_BUF_LEN);

    filelen = image_size;
    flashaddr = 0;
    left = filelen;

    while (left > 0)
    {
        if (left > TMP_BUF_LEN)
        {
            len = TMP_BUF_LEN;
        }
        else
        {
            len = left;
        }

        hal_flash_read(HAL_PARTITION_OTA_TEMP, &flashaddr, (uint8_t *)tmpbuf, len);
        crcout = crc16_maxim_multi((char *)tmpbuf, len, crcout);
        left -= len;
    }
    
    aos_free(tmpbuf);    
    return ~crcout;
}

static int ota_init(void *something)
{
    LOGW("mars", "多模块ota: 开始 ota_init");

    int ret = 0;
    ota_boot_param_t *param = (ota_boot_param_t*)something;
    _offset = param->off_bp;

    hal_logic_partition_t *part_info = hal_flash_get_info(HAL_PARTITION_OTA_TEMP);
    LOGW("mars", "ota init: ota分区大小=%dKB 新固件大小=%dKB 断点续传位置=%d", part_info->partition_length/1024, param->len/1024, param->off_bp);
    if (part_info->partition_length < param->len || param->len == 0)
    {
        LOGE("mars", "ota init: 升级文件过大!!! error");
        ret = OTA_PARAM_FAIL;
        return ret;
    }

    uint64_t erase_time = aos_now_ms();
    LOGW("mars", "ota init: 开始擦除OTA分区......(分区大小 = %d KB)", part_info->partition_length/1024);
    if (param->off_bp == 0)
    {
        int ret = 0;
        int len = part_info->partition_length;
        int offset = _offset;
        while (len > 0)
        {
            ret = hal_flash_erase(HAL_PARTITION_OTA_TEMP, offset, 4096);
            if (ret != 0)
            {
                LOGE("mars", "ota init: erase failed!!!");
                ret = OTA_INIT_FAIL;
                return ret;
            }
            offset += 4096;
            len -= 4096;
            aos_msleep(1);
        }
    }
    LOGW("mars", "ota init: ota分区擦除完成 (耗时%d秒)", (aos_now_ms()-erase_time)/1000);
    return ret;
}

static int ota_write(int *off, char *in_buf, int in_buf_len)
{
    int32_t ret = hal_flash_write(HAL_PARTITION_OTA_TEMP, (uint32_t *)&_offset, (uint8_t *)in_buf, in_buf_len);
    if (ret == 0)
        LOGI("mars", "多模块ota: 开始ota write 本次写入=%d 累计写入=%d", in_buf_len, _offset);
    else
        LOGE("mars", "多模块ota: 开始ota write fail!!!");
    return ret;
}

static int ota_read(int *off, char *out_buf, int out_buf_len)
{
    LOGI("mars", "多模块ota: 开始ota_read");
    return hal_flash_read(HAL_PARTITION_OTA_TEMP, (uint32_t *)off, (uint8_t *)out_buf, out_buf_len);
}

typedef struct
{
    uint32_t dst_adr;
    uint32_t src_adr;
    uint32_t siz;
    uint16_t crc;
} ota_hdr_t;

static int hal_ota_switch(uint32_t ota_len, uint16_t ota_crc)
{
    uint32_t addr = 0;
    ota_hdr_t ota_hdr = {
        .dst_adr = 0xA000,
        .src_adr = 0x100000,
        .siz = ota_len,
        .crc = ota_crc,
    };

    hal_flash_write(HAL_PARTITION_PARAMETER_1, &addr, (uint8_t *)&ota_hdr, sizeof(ota_hdr));

    return 0;
}

typedef enum{
    M_MODULEOTA_STATRT = 0,
    M_MODULEOTA_ING,
    M_MODULEOTA_SUCCESS,
    M_MODULEOTA_FAILD,
    M_MODULEOTA_GETVERSION,
    M_MODULEOTA_NULL,
}m_moduleota_step_en;

typedef struct
{
    m_moduleota_step_en ota_step;
    uint8_t ota_module;

    uint16_t pack_size;
    uint16_t pack_sum;
    uint16_t pack_cnt;

    uint16_t img_crc;
    uint32_t img_size;
}m_moduleota_st;

static m_moduleota_st g_ota_status = 
{
    .ota_step  = M_MODULEOTA_STATRT,
    .pack_size = 128,
    .pack_sum  = 0,
    .pack_cnt  = 0
};

void mars_ota_status_init(uint8_t target)
{
    g_ota_status.ota_step   = M_MODULEOTA_STATRT;
    g_ota_status.ota_module = target;
    g_ota_status.pack_size  = 128;
    g_ota_status.pack_sum   = 0;
    g_ota_status.pack_cnt   = 0;
    g_ota_status.img_crc    = 0;
    g_ota_status.img_size   = 0;

    LOGI("mars", "模块ota: g_ota_status 数据归零");
}

bool is_module_ota()
{
    if (g_ota_status.ota_step == M_MODULEOTA_SUCCESS)
        return false;

    if (g_ota_status.img_crc != 0 && g_ota_status.img_size != 0 && request_flag)    
        return true;
    else 
        return false;
}

void del_dis_fw_proc(uint8_t ver)
{
    if (g_ota_status.ota_step != M_MODULEOTA_SUCCESS)
    {
        LOGI("mars", "模块ota: del_pwr_fw_proc error (g_ota_status.ota_step=%d)", g_ota_status.ota_step);
        return;
    }

    char version_str[8] = {0};
    int  len = sizeof(version_str);
    if (HAL_Kv_Get("ota_m_version_cloud", version_str, &len) == 0 && len == 3)  //0.8 9.C
    {
        version_str[1] = version_str[2];
        version_str[2] = 0;
        uint8_t cloud_ver_hex = (uint8_t)(htoi(version_str));

        LOGI("mars", "模块ota: 固件版本 cloud=0x%02X local=0x%02X", cloud_ver_hex, ver);
        if ((cloud_ver_hex == ver) && (g_ota_status.ota_module == 0x00))
        {
            mars_del_ota_para();
            mars_ota_status_init(0);
            // aos_msleep(5000);
            // LOGW("mars", "删除显示板固件完毕,开始复位");
            // ota_reboot();
            do_delay_reset(); //ota_reboot();
        }
    }
}

void del_pwr_fw_proc(uint8_t ver)
{
    if (g_ota_status.ota_step != M_MODULEOTA_SUCCESS)
    {
        LOGI("mars", "模块ota: del_pwr_fw_proc error (g_ota_status.ota_step=%d)", g_ota_status.ota_step);
        return;
    }

    char version_str[8] = {0};
    int  len = sizeof(version_str);
    if (HAL_Kv_Get("ota_m_version_cloud", version_str, &len) == 0 && len == 3)  //0.8 9.C
    {
        version_str[1] = version_str[2];
        version_str[2] = 0;
        uint8_t cloud_ver_hex = (uint8_t)(htoi(version_str));

        LOGI("mars", "模块ota: 固件版本 cloud=0x%02X local=0x%02X", cloud_ver_hex, ver);
        if ((cloud_ver_hex == ver) && (g_ota_status.ota_module == 0x01))
        {
            mars_del_ota_para();
            mars_ota_status_init(0);
            // aos_msleep(5000);
            // LOGW("mars", "删除电源板固件完毕,开始复位");
            // ota_reboot();
            do_delay_reset(); //ota_reboot();
        }
    }
}

uint32_t g_flashaddr = 0, g_len = 0, g_left = 0;
void moduleota_step(void)
{
    uint8_t ota_buf[1024] = {0};
    uint16_t buf_len = 0;

    ota_seq = uart_get_seq_mid();
    if (g_moduleota_taskstatus)
    {
        switch (g_ota_status.ota_step)
        {
            case (M_MODULEOTA_STATRT):
            {
                if (g_ota_status.img_size % g_ota_status.pack_size)                
                    g_ota_status.pack_sum = (g_ota_status.img_size / g_ota_status.pack_size) + 1;                
                else                
                    g_ota_status.pack_sum = (g_ota_status.img_size / g_ota_status.pack_size);
                
                //msg send
                ota_buf[buf_len++] = prop_ota_request;
                ota_buf[buf_len++] = g_ota_status.ota_module;   //ota target
                ota_buf[buf_len++] = 0x00;                      //ota subcmd (请求升级)
                ota_buf[buf_len++] = (uint8_t)(g_ota_status.img_size >> 24);
                ota_buf[buf_len++] = (uint8_t)(g_ota_status.img_size >> 16);
                ota_buf[buf_len++] = (uint8_t)(g_ota_status.img_size >> 8);
                ota_buf[buf_len++] = (uint8_t)(g_ota_status.img_size );
                //version
                // char version_str[10] = {0};
                // int len = 10;
                // HAL_Kv_Get("ota_m_version_cloud", version_str, &len);
                // if (len){   //"0.1"
                //     version_str[1] = 0;
                //     uint8_t ver_h = (uint8_t)atoi(version_str);
                //     uint8_t ver_l = (uint8_t)atoi(version_str+2);
                //     ota_buf[buf_len++] = (uint8_t)(ver_h << 4) | (ver_l & 0x0F);
                // }else{
                //     ota_buf[buf_len++] = 0x00;   
                // }

                char version_str[8] = {0};
                int  len = sizeof(version_str);
                if (HAL_Kv_Get("ota_m_version_cloud", version_str, &len) == 0 && len == 3)  //0.8 9.C
                {
                    version_str[1] = version_str[2];
                    version_str[2] = 0;
                    uint8_t cloud_ver_hex = (uint8_t)(htoi(version_str));
                    ota_buf[buf_len++]    = cloud_ver_hex;
                    LOGW("mars", "模块ota: 云端下发版本 = 0x%02X", cloud_ver_hex);
                }
                else
                {
                    ota_buf[buf_len++] = 0x00;
                    LOGE("mars", "模块ota: 云端下发版本格式错误!!! %s", version_str);
                }


                ota_buf[buf_len++] = (uint8_t)(g_ota_status.img_crc >> 8);
                ota_buf[buf_len++] = (uint8_t)(g_ota_status.img_crc);
                ota_buf[buf_len++] = (uint8_t)(g_ota_status.pack_size >> 8);
                ota_buf[buf_len++] = (uint8_t)(g_ota_status.pack_size);
                Mars_uartmsg_send(0x0A, ota_seq, ota_buf, buf_len, OTA_RESEND);
                LOGI("mars", "模块ota: 发送请求帧 (总大小=%d 总包数=%d 单包长度=%d)", g_ota_status.img_size, g_ota_status.pack_sum, g_ota_status.pack_size);
                g_ota_status.pack_cnt = 0;
                g_left = g_ota_status.img_size;
                // g_ota_status.ota_step = M_MODULEOTA_ING;
                g_flashaddr = 0;
                break;
            }
            case (M_MODULEOTA_ING):
            {
                ota_buf[buf_len++] = prop_ota_request;
                ota_buf[buf_len++] = g_ota_status.ota_module;   //ota target
                ota_buf[buf_len++] = 0x01;                      //ota subcmd (升级数据)
                ota_buf[buf_len++] = (uint8_t)(g_ota_status.pack_sum >> 8);
                ota_buf[buf_len++] = (uint8_t)(g_ota_status.pack_sum );
                ota_buf[buf_len++] = (uint8_t)(g_ota_status.pack_cnt >> 8);
                ota_buf[buf_len++] = (uint8_t)(g_ota_status.pack_cnt);
                if (g_left > 0)
                {
                    g_len = (g_left > g_ota_status.pack_size)? g_ota_status.pack_size : g_left;
                    ota_buf[buf_len++] = (uint8_t)(g_len >> 8);
                    ota_buf[buf_len++] = (uint8_t)(g_len & 0xFF);
                    if (hal_flash_read(HAL_PARTITION_OTA_TEMP, &g_flashaddr, (uint8_t *)ota_buf+buf_len, g_len) != 0)
                    {
                        LOGE("mars", "模块ota: 读取flash failed!!! (len=%d)", g_len);
                    }
                    buf_len += g_len;
                    //aos_msleep(1000);
                    Mars_uartmsg_send(0x0A, ota_seq, ota_buf, buf_len, OTA_RESEND);

                    g_left -= g_len;
                    g_ota_status.pack_cnt++;
                    LOGI("mars", "模块ota: 本次发送=%d (已发送/总大小=%d/%d 当前包/总包数: %d/%d)",
                            g_len, g_ota_status.img_size-g_left, g_ota_status.img_size, g_ota_status.pack_cnt, g_ota_status.pack_sum);
                }
                else
                {
                    g_ota_status.ota_step = M_MODULEOTA_SUCCESS;
                    //结束升级
                    buf_len = 0;
                    ota_buf[buf_len++] = prop_ota_request;
                    ota_buf[buf_len++] = g_ota_status.ota_module;   //ota target
                    ota_buf[buf_len++] = 0x04;                      //ota subcmd (升级结束)
                    aos_msleep(1500);
                    Mars_uartmsg_send(0x0A, ota_seq, ota_buf, buf_len, OTA_RESEND);
                    LOGI("mars", "模块ota: 发送结束帧 (M_MODULEOTA_SUCCESS 1)");
                    g_moduleota_taskstatus = false;
                    //mars_del_ota_para();
                    //mars_ota_status_init(0);
                    // do_otamodule_reboot();
                }

                break;
            }

            case (M_MODULEOTA_SUCCESS):
            {
                ota_buf[buf_len++] = prop_ota_request;
                ota_buf[buf_len++] = g_ota_status.ota_module;   //ota target
                ota_buf[buf_len++] = 0x04;                      //ota subcmd (升级结束)
                Mars_uartmsg_send(0x0A, ota_seq, ota_buf, buf_len, OTA_RESEND);
                LOGI("mars", "模块ota: 发送结束帧 (M_MODULEOTA_SUCCESS 2)");
                g_moduleota_taskstatus = false;
                mars_del_ota_para();
                // HAL_Reboot();
                // do_otamodule_reboot();
                break;
            }

            case (M_MODULEOTA_FAILD):
            {
                ota_buf[buf_len++] = prop_ota_request;
                ota_buf[buf_len++] = g_ota_status.ota_module;   //ota target
                ota_buf[buf_len++] = 0x04;                      //ota subcmd (升级结束)
                Mars_uartmsg_send(0x0A, ota_seq, ota_buf, buf_len, OTA_RESEND);
                LOGI("mars", "模块ota: 发送结束帧 (M_MODULEOTA_FAILD)");
                g_moduleota_taskstatus = false;
                extern void mars_ota_status(int status);
                int err = OTA_REBOOT_FAIL;
                mars_ota_status(err);
                // HAL_Reboot();
                // do_otamodule_reboot();
                break;
            }
            default:
                break;
        }
    }
}

void mars_store_ota_para()
{
    aos_kv_del("ota_module_flag");
    aos_kv_del("ota_module");
    aos_kv_del("pack_size");
    aos_kv_del("pack_sum");
    aos_kv_del("img_crc");
    aos_kv_del("img_size");

    aos_kv_set("ota_module", &g_ota_status.ota_module, 1, 1);
    aos_kv_set("pack_size",  &g_ota_status.pack_size,  2, 1);
    aos_kv_set("pack_sum",   &g_ota_status.pack_sum,   2, 1);
    aos_kv_set("img_crc",    &g_ota_status.img_crc,    2, 1);
    aos_kv_set("img_size",   &g_ota_status.img_size,   4, 1);

    uint8_t ota_module_flag = 1;
    aos_kv_set("ota_module_flag",   &ota_module_flag, 1, 1);

    LOGW("mars", "模块ota: para写入flash成功 ");
}

void mars_get_ota_para()
{
    uint8_t ota_module_flag = 1;
    int len = 1;
    int ret = aos_kv_get("ota_module_flag", &ota_module_flag, &len);
    if (ret == 0 && len > 0 && ota_module_flag == 1) 
    {
        LOGW("mars", "模块ota: flash中发现可升级固件!!!!!!");  //crc16=0xC348 len=70864

        len = 1;
        aos_kv_get("ota_module", &g_ota_status.ota_module, &len);
        LOGW("mars", "固件信息: g_ota_status.ota_module = %d", g_ota_status.ota_module);

        len = 2;
        aos_kv_get("pack_size", &g_ota_status.pack_size, &len);
        LOGW("mars", "固件信息: g_ota_status.pack_size  = %d", g_ota_status.pack_size);

        len = 2;
        aos_kv_get("pack_sum", &g_ota_status.pack_sum, &len);
        LOGW("mars", "固件信息: g_ota_status.pack_sum   = %d", g_ota_status.pack_sum);

        len = 2;
        aos_kv_get("img_crc", &g_ota_status.img_crc, &len);
        LOGW("mars", "固件信息: g_ota_status.img_crc    = 0x%02X", g_ota_status.img_crc);

        len = 4;
        aos_kv_get("img_size", &g_ota_status.img_size, &len);
        LOGW("mars", "固件信息: g_ota_status.img_size   = %d", g_ota_status.img_size);

        //request_flag = true;
    }
    else
    {
        LOGW("mars", "模块ota: flash中不存在可升级固件");
    }
}

void mars_del_ota_para()
{
    aos_kv_del("ota_module_flag");
    aos_kv_del("ota_module");
    aos_kv_del("pack_size");
    aos_kv_del("pack_sum");
    aos_kv_del("img_crc");
    aos_kv_del("img_size");

    LOGI("mars", "模块ota: mars_del_ota_para OTA掉电数据归零");
}

void mars_otamodule_rsp(uint16_t seq, uint8_t cmd, uint8_t *buf, uint16_t len)
{
    uint8_t prop = buf[0];      //属性码：0xF9-升级指令 0xFA-升级应答
    uint8_t sub_cmd  = buf[1];  //命令字：0x00-肯定应答 0x01-否定应答

    if (cmd == cmd_ota_0B && prop == prop_ota_response)
    {
        LOGI("mars", "模块ota: 收到应答帧 (cmd=0x%02X prop=0x%02X)", cmd, prop);
    }
    else if (cmd == cmd_ota_0B && prop == prop_ota_request)
    {
        LOGI("mars", "模块ota: 收到请求帧 (cmd=0x%02X prop=0x%02X)", cmd, prop);
    }
    else 
    {
        LOGE("mars", "模块ota: 收到错误帧 (cmd=0x%02X prop=0x%02X)", cmd, prop);
        return;
    }

    if(prop == prop_ota_response)  //命令码0x0B 属性码0xFA
    {
        if (sub_cmd == 0)
        {
            output_hex_string("收到ota应答: 肯定应答 ", buf, len);
            uint8_t typ = buf[2];
            LOGW("mars", "模块ota: 模块ota类型 = %s", (typ == 0)? "备份类型" : "boot类型");
            if(g_ota_status.ota_step == M_MODULEOTA_STATRT)
            {
                if (!request_flag)  //E6E600110A000CF90000000114D099C3480100F6D06E6E  E6E600110B0005FA000100009DF76E6E
                {
                    //mars_store_ota_para();
                    LOGI("mars", "开始等待电控板进入boot......");
                    //request_flag = true;
                }
                else            // E6E600120A000CF90000000114D099C348010007906E6E E6E600120B0005FA000100006DE36E6E
                {
                    g_ota_status.ota_step = M_MODULEOTA_ING;
                    LOGI("mars", "开始发送升级数据");
                    moduleota_step();
                }
            }
            else if (g_ota_status.ota_step == M_MODULEOTA_ING)
            {
                moduleota_step();
            }
        }
        else
        {
            output_hex_string("收到ota应答: 否认应答!!!!!! ", buf, len);
            if(g_ota_status.ota_step == M_MODULEOTA_STATRT && 0x05 == buf[2])
            {
                uint16_t pack_size = ((uint16_t)buf[3] << 8) | (uint16_t)buf[4];
                LOGE("mars", "电控板否认应答: OTA中建议每包下发长度: %d", pack_size);
                g_ota_status.pack_size = pack_size;
                g_ota_status.ota_step = M_MODULEOTA_STATRT;
                moduleota_step();
                LOGW("mars", "app阶段: 通讯板再次发送升级信息...(升级对象: %s)", ota_target_desc[g_ota_status.ota_module]);
            }
        }
    }
    else if(prop == prop_ota_request) //命令码0x0B 属性码0xF9
    {
        output_hex_string("收到ota请求: ", buf, len);  //E6E600010B0003F90010FD3F6E6E  E6E600010A0005FA0001000091FB6E6E
        if (buf[2] == 0x10)
        {
            if (buf[1] < 3)
                LOGW("mars", "电控板请求升级 %s", ota_target_desc[buf[1]]);
            else 
                LOGE("mars", "电控板请求升级目标错误");

            if (g_ota_status.ota_module == buf[1] && g_ota_status.img_crc != 0 && g_ota_status.img_size > 0)
            {
                uint8_t buff[] = {prop_ota_response, 0x00, 0x01, 0x00, 0x00};  //属性码 命令字(肯定应答) OTA类型(Boot类型) 预留1 预留2
                Mars_uartmsg_send(cmd_ota_0A, seq, buff, sizeof(buff), 0);

                request_flag = true;
                ota_boot_manual();
            }
            else
            {
                uint8_t buff[] = {prop_ota_response, 0x01, 0x09, 0x00, 0x00};  //属性码 命令字(肯定应答) OTA类型(Boot类型) 预留1 预留2
                Mars_uartmsg_send(cmd_ota_0A, seq, buff, sizeof(buff), 0);

                LOGE("mars", "error, 通讯板中不存在目标固件");
            }
        }
    }
    else
    {

    }
}

static int ota_boot(void *something)
{
    LOGW("mars", "多模块ota: 开始 ota_boot");

    int ret = 0;
    ota_boot_param_t *param = (ota_boot_param_t *)something;
    if (param == NULL)
    {
        LOGI("mars", "ota_boot: failed!!!");
        ret = OTA_REBOOT_FAIL;
        return ret;
    }

    if (param->res_type == OTA_FINISH)
    {
        LOGW("mars", "ota_boot: 固件下载完成");
        if (param->upg_flag == OTA_DIFF)
        {

        }
        else
        {
            param->crc = ota_image_crc(param->len);
            LOGI("mars", "ota_boot: 固件CRC计算完成 crc16=0x%02X len=%d", param->crc, param->len);

            g_ota_status.img_crc   = param->crc;
            g_ota_status.img_size  = param->len;
            //g_ota_status.pack_size = 256; 
            g_ota_status.ota_step  = M_MODULEOTA_STATRT;
            g_moduleota_taskstatus = true;
            moduleota_step();
            LOGW("mars", "app阶段: 通讯板发送升级信息...(升级对象: %s)", ota_target_desc[g_ota_status.ota_module]);
            mars_store_ota_para();
        }

    }
    return ret;
}

void ota_boot_manual(void)
{
    g_moduleota_taskstatus = true;    
    g_ota_status.ota_step  = M_MODULEOTA_STATRT;        
    moduleota_step();
    LOGW("mars", "boot阶段: 通讯板发送升级信息...(升级对象: %s)", ota_target_desc[g_ota_status.ota_module]);
}

static int ota_rollback(void *something)
{
    LOGI("mars", "多模块ota: 开始 ota_rollback");
    return 0;
}

static int ota_init_1(void *something)
{
    ota_init(something);
    mars_ota_status_init(0);
}

static int ota_write_1(int *off, char *in_buf, int in_buf_len)
{
    ota_write(off, in_buf, in_buf_len);
}

static int ota_read_1(int *off, char *out_buf, int out_buf_len)
{
    ota_read(off, out_buf, out_buf_len);
}

static int ota_boot_1(void *something)
{
    g_ota_status.ota_module = 0x00; //显示板
    ota_boot(something);
}

static int ota_rollback_1(void *something)
{
    ota_rollback(something);
}

static const char *ota_get_version_1(unsigned char dev_type)
{
    char version_str[10] = {0};
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();    
    sprintf(version_str, "%X.%X", mars_template_ctx->status.ElcSWVersion>>4, mars_template_ctx->status.ElcSWVersion & 0x0F);
    return version_str;
}

ota_hal_module_t mars_ota_module_1 =
{      
    .init       = ota_init_1,
    .write      = ota_write_1,
    .read       = ota_read_1,
    .boot       = ota_boot_1,
    .rollback   = ota_rollback_1,
    .version    = ota_get_version_1,
};

static int ota_init_2(void *something)
{
    ota_init(something);
    mars_ota_status_init(1);
}

static int ota_write_2(int *off, char *in_buf, int in_buf_len)
{
    ota_write(off, in_buf, in_buf_len);
}

static int ota_read_2(int *off, char *out_buf, int out_buf_len)
{
    ota_read(off, out_buf, out_buf_len);
}

static int ota_boot_2(void *something)
{
    g_ota_status.ota_module = 0x01; //电源板
    ota_boot(something);
}

static int ota_rollback_2(void *something)
{
    ota_rollback(something);
}

static const char *ota_get_version_2(unsigned char dev_type)
{
    char version_str[10] = {0};
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();    
    sprintf(version_str, "%X.%X", mars_template_ctx->status.PwrSWVersion>>4, mars_template_ctx->status.PwrSWVersion & 0x0F);
    return version_str;
}

ota_hal_module_t mars_ota_module_2 = 
{          
    .init       = ota_init_2,
    .write      = ota_write_2,
    .read       = ota_read_2,
    .boot       = ota_boot_2,
    .rollback   = ota_rollback_2,
    .version    = ota_get_version_2,
};

static void mars_otamodule_callback(void *arg1, void *arg2)
{
    g_ota_status.ota_step = M_MODULEOTA_FAILD;
    moduleota_step();
}


/*
[2023-05-11 09:58:12.059] <I> mars  : 串口通讯 wifi ---> ecb: [23] E6E600100A000CF90000000127209047C80080BCF36E6E
[2023-05-11 09:58:12.062] <I> mars  : 模块ota: 发送请求帧 (总大小=75552 总包数=591 单包长度=128)
[2023-05-11 09:58:12.063] <W> mars  : app阶段: 通讯板发送升级信息...(升级对象: 显示板)
[2023-05-11 09:58:12.234] <I> mars  : 串口通讯 wifi <--- ecb(腰部板 seq=16): [16] E6E600100B0005FA000100000DFA6E6E
[2023-05-11 09:58:12.249] <I> mars  : 开始删除对应的ota发送报文 seq=0x10
[2023-05-11 09:58:12.250] <I> mars  : 模块ota: 收到应答帧 (cmd=0x0B prop=0xFA)
[2023-05-11 09:58:12.251] <I> mars  : 收到ota应答: 肯定应答 [5] FA00010000
[2023-05-11 09:58:12.251] <W> mars  : 模块ota: 模块ota类型 = boot类型
[2023-05-11 09:58:12.297] <W> mars  : 模块ota: para写入flash成功
[2023-05-11 09:58:12.298] <I> mars  : 开始等待电控板进入boot......


[2023-05-11 09:58:15.056] <I> mars  : 串口通讯 wifi <--- ecb(腰部板 seq=1): [14] E6E600010B0003F90010FD3F6E6E
[2023-05-11 09:58:15.099] <I> mars  : 模块ota: 收到请求帧 (cmd=0x0B prop=0xF9)
[2023-05-11 09:58:15.100] <I> mars  : 收到ota请求: [3] F90010
[2023-05-11 09:58:15.100] <W> mars  : 电控板请求升级 显示板
[2023-05-11 09:58:15.101] <I> mars  : 串口通讯 wifi ---> ecb: [16] E6E600010A0005FA0001000091FB6E6E


[2023-05-11 09:58:15.105] <I> mars  : 串口通讯 wifi ---> ecb: [23] E6E600110A000CF90000000127209047C800802C326E6E
[2023-05-11 09:58:15.106] <I> mars  : 模块ota: 发送请求帧 (总大小=75552 总包数=591 单包长度=128)
[2023-05-11 09:58:15.107] <W> mars  : boot阶段: 通讯板发送升级信息...(升级对象: 显示板)
[2023-05-11 09:58:15.268] <I> mars  : 串口通讯 wifi <--- ecb(腰部板 seq=17): [16] E6E600110B0005FA000100009DF76E6E
[2023-05-11 09:58:15.308] <I> mars  : 开始删除对应的ota发送报文 seq=0x11
[2023-05-11 09:58:15.309] <I> mars  : 模块ota: 收到应答帧 (cmd=0x0B prop=0xFA)
[2023-05-11 09:58:15.309] <I> mars  : 收到ota应答: 肯定应答 [5] FA00010000
[2023-05-11 09:58:15.311] <W> mars  : 模块ota: 模块ota类型 = boot类型
[2023-05-11 09:58:15.311] <I> mars  : 开始发送升级数据


[2023-05-11 09:58:16.325] <I> mars  : 串口通讯 wifi ---> ecb: [148] E6E600120A0089F90001024F0000008070190020494100005141000053410000000000000000000000000000000000000000000000000000000000005B41000000000000000000005F41000031300100634100006541000067410000694100006B4100006D4100006F41000019480000814800007541000079490000D54900007B4100007D41000089300100814100000D7A6E6E
[2023-05-11 09:58:16.327] <I> mars  : 模块ota: 本次发送=128 (已发送/总大小=128/75552 当前包/总包数: 1/591)
[2023-05-11 09:58:16.472] <I> mars  : 串口通讯 wifi <--- ecb(腰部板 seq=18): [16] E6E600120B0005FA000100006DE36E6E
[2023-05-11 09:58:16.481] <I> mars  : 开始删除对应的ota发送报文 seq=0x12
[2023-05-11 09:58:16.482] <I> mars  : 模块ota: 收到应答帧 (cmd=0x0B prop=0xFA)
[2023-05-11 09:58:16.483] <I> mars  : 收到ota应答: 肯定应答 [5] FA00010000
[2023-05-11 09:58:16.484] <W> mars  : 模块ota: 模块ota类型 = boot类型

[2023-05-11 09:58:17.498] <I> mars  : 串口通讯 wifi ---> ecb: [148] E6E600130A0089F90001024F00010080834100008541000087410000894100008B4100008D410000954A000091410000934100009541000097410000994100009B4100009D4100009F410000A141000000F002F800F030F80CA030C8083824182D18A246671EAB4654465D46AC4201D100F022F87E460F3E0FCCB6460126334200D0FB1AA246AB4633431847F82501003A7B6E6E
[2023-05-11 09:58:17.501] <I> mars  : 模块ota: 本次发送=128 (已发送/总大小=256/75552 当前包/总包数: 2/591)
[2023-05-11 09:58:17.666] <I> mars  : 串口通讯 wifi <--- ecb(腰部板 seq=19): [16] E6E600130B0005FA00010000FDEE6E6E
[2023-05-11 09:58:17.702] <I> mars  : 开始删除对应的ota发送报文 seq=0x13
[2023-05-11 09:58:17.703] <I> mars  : 模块ota: 收到应答帧 (cmd=0x0B prop=0xFA)
[2023-05-11 09:58:17.703] <I> mars  : 收到ota应答: 肯定应答 [5] FA00010000
[2023-05-11 09:58:17.704] <W> mars  : 模块ota: 模块ota类型 = boot类型

[2023-05-11 10:10:20.728] <I> mars  : 串口通讯 wifi ---> ecb: [52] E6E6026C0A0029F90001024F024E0020CC46000020670100BC060020B41200000441000001FF01D502DE01C2FF000000E3106E6E
[2023-05-11 10:10:20.730] <I> mars  : 模块ota: 本次发送=32 (已发送/总大小=75552/75552 当前包/总包数: 591/591)
[2023-05-11 10:10:20.908] <I> mars  : 串口通讯 wifi <--- ecb(腰部板 seq=108): [16] E6E6006C0B0005FA00010000CFCA6E6E
[2023-05-11 10:10:20.931] <I> mars  : 开始删除对应的ota发送报文 seq=0x6C
[2023-05-11 10:10:20.931] <I> mars  : 模块ota: 收到应答帧 (cmd=0x0B prop=0xFA)
[2023-05-11 10:10:20.932] <I> mars  : 收到ota应答: 肯定应答 [5] FA00010000
[2023-05-11 10:10:20.933] <W> mars  : 模块ota: 模块ota类型 = boot类型

[2023-05-11 10:10:20.934] <I> mars  : 串口通讯 wifi ---> ecb: [14] E6E6026D0A0003F90004FC136E6E
[2023-05-11 10:10:20.935] <I> mars  : 模块ota: 发送结束帧 (M_MODULEOTA_SUCCESS 1)
[2023-05-11 10:10:20.962] <I> mars  : 模块ota: mars_del_ota_para OTA掉电数据归零
[2023-05-11 10:10:20.963] <I> mars  : 模块ota: g_ota_status 数据归零

*/