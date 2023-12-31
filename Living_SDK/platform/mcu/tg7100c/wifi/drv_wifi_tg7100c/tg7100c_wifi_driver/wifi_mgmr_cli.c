#include <stdio.h>
#include <string.h>
//#include <FreeRTOS.h>
//#include <task.h>
#include <os_hal.h>
#include <aos/cli.h>

#include <blog.h>
#include <bl_wifi.h>
#include <hal_sys.h>
#include <tg710x_fw_api.h>
#include <wifi_mgmr.h>
#include <wifi_mgmr_api.h>
#include <utils_hexdump.h>
#include <utils_tlv_bl.h>
#include <utils_getopt.h>
#include <wifi_mgmr_ext.h>

#define STATIC_CLI_CMD_ATTRIBUTE __attribute__((used, section(".static_cli_cmds")))

#define WIFI_AP_DATA_RATE_1Mbps      0x00
#define WIFI_AP_DATA_RATE_2Mbps      0x01
#define WIFI_AP_DATA_RATE_5_5Mbps    0x02
#define WIFI_AP_DATA_RATE_11Mbps     0x03
#define WIFI_AP_DATA_RATE_6Mbps      0x0b
#define WIFI_AP_DATA_RATE_9Mbps      0x0f
#define WIFI_AP_DATA_RATE_12Mbps     0x0a
#define WIFI_AP_DATA_RATE_18Mbps     0x0e
#define WIFI_AP_DATA_RATE_24Mbps     0x09
#define WIFI_AP_DATA_RATE_36Mbps     0x0d
#define WIFI_AP_DATA_RATE_48Mbps     0x08
#define WIFI_AP_DATA_RATE_54Mbps     0x0c

struct wifi_ap_data_rate {
    uint8_t data_rate;
    const char *val;
};

static const struct wifi_ap_data_rate data_rate_list[] = {
    {WIFI_AP_DATA_RATE_1Mbps, "1.0 Mbit/s, 20Mhz"},
    {WIFI_AP_DATA_RATE_2Mbps, "2.0 Mbit/s, 20Mhz"},
    {WIFI_AP_DATA_RATE_5_5Mbps, "5.5 Mbit/s, 20Mhz"},
    {WIFI_AP_DATA_RATE_11Mbps, "11.0 Mbit/s, 20Mhz"},
    {WIFI_AP_DATA_RATE_6Mbps, "6.0 Mbit/s, 20Mhz"},
    {WIFI_AP_DATA_RATE_9Mbps, "9.0 Mbit/s, 20Mhz"},
    {WIFI_AP_DATA_RATE_12Mbps, "12.0 Mbit/s, 20Mhz"},
    {WIFI_AP_DATA_RATE_18Mbps, "18.0 Mbit/s, 20Mhz"},
    {WIFI_AP_DATA_RATE_24Mbps, "24.0 Mbit/s, 20Mhz"},
    {WIFI_AP_DATA_RATE_36Mbps, "36.0 Mbit/s, 20Mhz"},
    {WIFI_AP_DATA_RATE_48Mbps, "48.0 Mbit/s, 20Mhz"},
    {WIFI_AP_DATA_RATE_54Mbps, "54.0 Mbit/s, 20Mhz"},
};

static unsigned char char_to_hex(char asccode)
{
    unsigned char ret;

    if('0'<=asccode && asccode<='9')
        ret=asccode-'0';
    else if('a'<=asccode && asccode<='f')
        ret=asccode-'a'+10;
    else if('A'<=asccode && asccode<='F')
        ret=asccode-'A'+10;
    else
        ret=0;

    return ret;
}

static void chan_str_to_hex(uint8_t *sta_num, char *sta_str)
{
    int i, str_len, base=1;
    uint16_t val = 0;
    char *q;

    str_len = strlen(sta_str);
    q = sta_str;
    q[str_len] = '\0';
    for (i=0; i< str_len; i++) {
        val = val + char_to_hex(q[str_len-1-i]) * base;
        base = base * 10;
    }
    (*sta_num) = val;
    os_printf("sta_str: %s, str_len: %d, sta_num: %d, q: %s\r\n", sta_str, str_len, (*sta_num), q);

}

static void wifi_ap_sta_list_get_cmd(char *buf, int len, int argc, char **argv)
{
    int state = WIFI_STATE_UNKNOWN;
    uint8_t sta_cnt = 0, i, j;
    struct wifi_sta_basic_info sta_info;
    long long sta_time;
    uint8_t index = 0;

    wifi_mgmr_state_get(&state);
    if (!WIFI_STATE_AP_IS_ENABLED(state)){
        os_printf("wifi AP is not enabled, state = %d\r\n", state);
        return;
    }

    wifi_mgmr_ap_sta_cnt_get(&sta_cnt);
    if (!sta_cnt){
        os_printf("no sta connect current AP, sta_cnt = %d\r\n", sta_cnt);
        return;
    }

    memset(&sta_info, 0, sizeof(struct wifi_sta_basic_info));
    os_printf("sta list:\r\n");
    os_printf("-----------------------------------------------------------------------------------\r\n");
    os_printf("No.      StaIndex      Mac-Address       Signal      DateRate            TimeStamp\r\n");
    os_printf("-----------------------------------------------------------------------------------\r\n");
    for(i = 0;i < sta_cnt;i++){
        wifi_mgmr_ap_sta_info_get(&sta_info, i);
        if (!sta_info.is_used || (sta_info.sta_idx == 0xef)){
            continue;
        }

        sta_time = (long long)sta_info.tsfhi;
        sta_time = (sta_time << 32) | sta_info.tsflo;

        for(j = 0;j < sizeof(data_rate_list)/sizeof(data_rate_list[0]);j++) {
            if(data_rate_list[j].data_rate == sta_info.data_rate) {
                index = j;
                break;
            }
        }

        os_printf(" %u       "
            "   %u        "
            "%02X:%02X:%02X:%02X:%02X:%02X    "
            "%d      "
            "%s      "
            "0x%llx"
            "\r\n",
            i,
            sta_info.sta_idx,
            sta_info.sta_mac[0],
            sta_info.sta_mac[1],
            sta_info.sta_mac[2],
            sta_info.sta_mac[3],
            sta_info.sta_mac[4],
            sta_info.sta_mac[5],
            sta_info.rssi,
            data_rate_list[index].val,
            sta_time
        );
    }
}

static void wifi_ap_sta_delete_cmd(char *buf, int len, int argc, char **argv)
{
    int state = WIFI_STATE_UNKNOWN;
    uint8_t sta_cnt = 0;
    struct wifi_sta_basic_info sta_info;
    uint8_t sta_num = 0;

    if (2 != argc) {
        os_printf("[USAGE]: %s sta_num\r\n", argv[0]);
        return;
    }

    wifi_mgmr_state_get(&state);
    if (!WIFI_STATE_AP_IS_ENABLED(state)){
        os_printf("wifi AP is not enabled, state = %d\r\n", state);
        return;
    }

    os_printf("Delete Sta No.%s \r\n", argv[1]);
    chan_str_to_hex(&sta_num, argv[1]);
    os_printf("sta num = %d \r\n", sta_num);

    wifi_mgmr_ap_sta_cnt_get(&sta_cnt);
    if (!sta_cnt || (sta_num > sta_cnt)){
        os_printf("no valid sta in list or sta idx(%d) is invalid\r\n", sta_cnt);
        return;
    }

    memset(&sta_info, 0, sizeof(struct wifi_sta_basic_info));
    wifi_mgmr_ap_sta_info_get(&sta_info, sta_num);
    if (!sta_info.is_used || (sta_info.sta_idx == 0xef)){
        os_printf("No.%d sta is invalid\r\n", sta_num);
        return;
    }

    os_printf("sta info: No.%u,"
        "sta_idx = %u,"
        "mac = %02X:%02X:%02X:%02X:%02X:%02X,"
        "rssi = %d"
        "\r\n",
        sta_num,
        sta_info.sta_idx,
        sta_info.sta_mac[0],
        sta_info.sta_mac[1],
        sta_info.sta_mac[2],
        sta_info.sta_mac[3],
        sta_info.sta_mac[4],
        sta_info.sta_mac[5],
        sta_info.rssi
    );
    wifi_mgmr_ap_sta_delete(sta_info.sta_idx);
}

static void wifi_edca_dump_cmd(char *buf, int len, int argc, char **argv)
{
    uint8_t aifs = 0, cwmin = 0, cwmax = 0;
    uint16_t txop = 0;

    puts("EDCA Statistic:\r\n");

    bl60x_edca_get(API_AC_BK, &aifs, &cwmin, &cwmax, &txop);
    puts("  AC_BK:");
    os_printf("aifs %3u, cwmin %3u, cwmax %3u, txop %4u\r\n",
        aifs, cwmin, cwmax, txop
    );

    bl60x_edca_get(API_AC_BE, &aifs, &cwmin, &cwmax, &txop);
    puts("  AC_BE:");
    os_printf("aifs %3u, cwmin %3u, cwmax %3u, txop %4u\r\n",
        aifs, cwmin, cwmax, txop
    );

    bl60x_edca_get(API_AC_VI, &aifs, &cwmin, &cwmax, &txop);
    puts("  AC_VI:");
    os_printf("aifs %3u, cwmin %3u, cwmax %3u, txop %4u\r\n",
        aifs, cwmin, cwmax, txop
    );

    bl60x_edca_get(API_AC_VO, &aifs, &cwmin, &cwmax, &txop);
    puts("  AC_VO:");
    os_printf("aifs %3u, cwmin %3u, cwmax %3u, txop %4u\r\n",
        aifs, cwmin, cwmax, txop
    );
}

int wifi_mgmr_cli_powersaving_on()
{
    wifi_mgmr_api_fw_powersaving(2);
    return 0;
}

int wifi_mgmr_cli_scanlist_num(int *num)
{
  int i, j;

  for (i = 0, j = 0; i < sizeof(wifiMgmr.scan_items) / sizeof(wifiMgmr.scan_items[0]); i++) {
    if (wifiMgmr.scan_items[i].is_used) {
      j++;
    }
  }
  *num = j;

  return 0;
}

int wifi_mgmr_cli_scanlist_foreach(int (*cb)(void *arg1, void *arg2, char *ssid, uint8_t bssid[6], int8_t rssi, int channel, uint16_t auth),
                                   void *arg1,
                                   void *arg2)
{
  int i;

  for (i = 0; i < sizeof(wifiMgmr.scan_items) / sizeof(wifiMgmr.scan_items[0]); i++) {
    if (wifiMgmr.scan_items[i].is_used) {
      if (cb(arg1, arg2, wifiMgmr.scan_items[i].ssid,
             wifiMgmr.scan_items[i].bssid,
             wifiMgmr.scan_items[i].rssi,
             wifiMgmr.scan_items[i].channel,
             wifiMgmr.scan_items[i].auth)) {
        /*break on non zero*/
        break;
      }
    }
  }

  return 0;
}

int wifi_mgmr_cli_scanlist(void)
{
    int i;

    printf("cached scan list\r\n");
    printf("****************************************************************************************************\r\n");
    for (i = 0; i < sizeof(wifiMgmr.scan_items)/sizeof(wifiMgmr.scan_items[0]); i++) {
        if (wifiMgmr.scan_items[i].is_used && (!wifi_mgmr_scan_item_is_timeout(&wifiMgmr, &wifiMgmr.scan_items[i]))) {
            printf("index[%02d]: channel %02u, bssid %02X:%02X:%02X:%02X:%02X:%02X, rssi %3d, ppm abs:rel %3d : %3d, auth %20s, cipher:%12s, SSID %s\r\n",
                    i,
                    wifiMgmr.scan_items[i].channel,
                    wifiMgmr.scan_items[i].bssid[0],
                    wifiMgmr.scan_items[i].bssid[1],
                    wifiMgmr.scan_items[i].bssid[2],
                    wifiMgmr.scan_items[i].bssid[3],
                    wifiMgmr.scan_items[i].bssid[4],
                    wifiMgmr.scan_items[i].bssid[5],
                    wifiMgmr.scan_items[i].rssi,
                    wifiMgmr.scan_items[i].ppm_abs,
                    wifiMgmr.scan_items[i].ppm_rel,
                    wifi_mgmr_auth_to_str(wifiMgmr.scan_items[i].auth),
                    wifi_mgmr_cipher_to_str(wifiMgmr.scan_items[i].cipher),
                    wifiMgmr.scan_items[i].ssid
            );
        } else {
            printf("index[%02d]: empty\r\n", i);
        }
    }
    printf("----------------------------------------------------------------------------------------------------\r\n");
    return 0;
}

static void cmd_rf_dump(char *buf, int len, int argc, char **argv)
{
    //bl60x_fw_dump_data();
}

static void wifi_capcode_cmd(char *buf, int len, int argc, char **argv)
{
    int capcode = 0;

    if (2 != argc && 1 != argc) {
        os_printf("Usage: %s capcode\r\n", argv[0]);
        return;
    }

    /*get capcode*/
    if (1 == argc) {
        os_printf("Capcode %u is being used\r\n", hal_sys_capcode_get());
        return;
    }

    /*set capcode*/
    capcode = atoi(argv[1]);
    os_printf("Setting capcode to %d\r\n", capcode);

    if (capcode > 0) {
        hal_sys_capcode_update(capcode, capcode);
    }
}

static void wifi_bcnint_set(char *buf, int len, int argc, char **argv)
{
    uint16_t bcnint = 0;

    if (2 != argc) {
        printf("Usage: %s bcnint\r\n", argv[0]);
        return;
    }

    bcnint = atoi(argv[1]);
    printf("Setting beacon interval to %d\r\n", bcnint);

    if (bcnint > 0) {
        wifi_mgmr_beacon_interval_set(bcnint);
    }
}

static void wifi_scan_cmd(char *buf, int len, int argc, char **argv)
{
    uint16_t channel_num;
    uint16_t channels[MAX_FIXED_CHANNELS_LIMIT];
    int i;
    if (1 == argc) {
        wifi_mgmr_scan(NULL, NULL);
    }
    if (argc > 1) {
        channel_num = argc - 1;
        if (channel_num > MAX_FIXED_CHANNELS_LIMIT) {
            printf("---->scan fixed channels' number limit\r\n");
            return ;
        }
        for(i = 0; i < (argc - 1); i ++) {
            channels[i] = atoi(argv[i + 1]);
        }
        wifi_mgmr_scan_fixed_channels(NULL, NULL, channels, channel_num);
    }
}

static void wifi_scan_filter_cmd(char *buf, int len, int argc, char **argv)
{
    int filter = -1;

    if (2 != argc) {
        return;
    }

    filter = argv[1][0] == '1' ? 1 : 0;
    blog_info("Scan Filter %s\r\n", filter ? "enable" : "disdable");
    wifi_mgmr_scan_filter_hidden_ssid(filter);
}

static void wifi_sta_ip_info(char *buf, int len, int argc, char **argv)
{
    ip4_addr_t ip, gw, mask, dns1, dns2;
    int rssi;
    int8_t power_rate_table[38];


    wifi_mgmr_sta_ip_get(&ip.addr, &gw.addr, &mask.addr);
    wifi_mgmr_sta_dns_get(&dns1.addr, &dns2.addr);
    wifi_mgmr_rssi_get(&rssi);
    bl_tpc_power_table_get(power_rate_table);
    os_printf("RSSI:   %ddbm\r\n", rssi);
    os_printf("IP  :   %s \r\n", ip4addr_ntoa(&ip) );
    os_printf("MASK:   %s \r\n", ip4addr_ntoa(&mask));
    os_printf("GW  :   %s \r\n", ip4addr_ntoa(&gw));
    os_printf("DNS1:   %s \r\n", ip4addr_ntoa(&dns1));
    os_printf("DNS2:   %s \r\n", ip4addr_ntoa(&dns2));
    puts(  "Power Table (dbm):\r\n");
    puts(  "--------------------------------\r\n");
    os_printf("  11b: %u %u %u %u             (1Mbps 2Mbps 5.5Mbps 11Mbps)\r\n",
        power_rate_table[0],
        power_rate_table[1],
        power_rate_table[2],
        power_rate_table[3]
    );
    os_printf("  11g: %u %u %u %u %u %u %u %u (6Mbps 9Mbps 12Mbps 18Mbps 24Mbps 36Mbps 48Mbps 54Mbps)\r\n",
        power_rate_table[0 + 8],
        power_rate_table[1 + 8],
        power_rate_table[2 + 8],
        power_rate_table[3 + 8],
        power_rate_table[4 + 8],
        power_rate_table[5 + 8],
        power_rate_table[6 + 8],
        power_rate_table[7 + 8]
    );
    os_printf("  11n: %u %u %u %u %u %u %u %u (MCS0 ~ MCS7)\r\n",
        power_rate_table[0 + 16],
        power_rate_table[1 + 16],
        power_rate_table[2 + 16],
        power_rate_table[3 + 16],
        power_rate_table[4 + 16],
        power_rate_table[5 + 16],
        power_rate_table[6 + 16],
        power_rate_table[7 + 16]
    );
    puts(  "--------------------------------\r\n");
}

static uint8_t packet_raw[] = {
    0x48, 0x02,
    0x00, 0x00,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x33, 0x33, 0x33, 0x33, 0x33, 0x33,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00
};

static void cmd_wifi_raw_send(char *buf, int len, int argc, char **argv)
{
    static uint32_t seq = 0;

    packet_raw[sizeof(packet_raw) - 2] = ((seq << 4) & 0xFF);
    packet_raw[sizeof(packet_raw) - 1] = (((seq << 4) & 0xFF00) >> 8);
    seq++;

    if (wifi_mgmr_raw_80211_send(packet_raw, sizeof(packet_raw))) {
        puts("Raw send failed\r\n");
    } else {
        puts("Raw send succeed\r\n");
    }
}

static void wifi_disconnect_cmd(char *buf, int len, int argc, char **argv)
{
    wifi_mgmr_sta_disconnect();
    /*XXX Must make sure sta is already disconnect, otherwise sta disable won't work*/
    bl_os_thread_delay(1000);
    wifi_mgmr_sta_disable(NULL);
}

static void wifi_sta_ip_set_cmd(char *buf, int len, int argc, char **argv)
{
    /* sample input
     *
     * cmd_ip_set 192.168.1.212 255.255.255.0 192.168.1.1 114.114.114.114 114.114.114.114
     * 
     * */
    uint32_t ip, mask, gw, dns1, dns2;
    char addr_str[20];
    ip4_addr_t addr;

    if (6 != argc) {
        puts("Illegal CMD format\r\n");
        return;
    }
    ip = ipaddr_addr(argv[1]);
    mask = ipaddr_addr(argv[2]);
    gw = ipaddr_addr(argv[3]);
    dns1 = ipaddr_addr(argv[4]);
    dns2 = ipaddr_addr(argv[5]);

    ip4_addr_set_u32(&addr, ip);
    ip4addr_ntoa_r(&addr, addr_str, sizeof(addr_str));
    puts("IP  : ");
    puts(addr_str);
    puts("\r\n");

    ip4_addr_set_u32(&addr, mask);
    ip4addr_ntoa_r(&addr, addr_str, sizeof(addr_str));
    puts("MASK: ");
    puts(addr_str);
    puts("\r\n");

    ip4_addr_set_u32(&addr, gw);
    ip4addr_ntoa_r(&addr, addr_str, sizeof(addr_str));
    puts("GW  : ");
    puts(addr_str);
    puts("\r\n");

    ip4_addr_set_u32(&addr, dns1);
    ip4addr_ntoa_r(&addr, addr_str, sizeof(addr_str));
    puts("DNS1: ");
    puts(addr_str);
    puts("\r\n");

    ip4_addr_set_u32(&addr, dns2);
    ip4addr_ntoa_r(&addr, addr_str, sizeof(addr_str));
    puts("DNS2: ");
    puts(addr_str);
    puts("\r\n");

    wifi_mgmr_sta_ip_set(ip, mask, gw, dns1, dns2);
}

static void wifi_sta_ip_unset_cmd(char *buf, int len, int argc, char **argv)
{
    wifi_mgmr_sta_ip_unset();
}

static void wifi_connect_cmd(char *buf, int len, int argc, char **argv)
{
    wifi_interface_t wifi_interface;

    if (3 != argc) {
        os_printf("[USAGE]: %s [ssid] [password]\r\n", argv[0]);
        return;
    }

    wifi_interface = wifi_mgmr_sta_enable();
    wifi_mgmr_sta_connect(wifi_interface, argv[1], argv[2], NULL, NULL, 0, 0);
}

static void wifi_sta_get_state_cmd(char *buf, int len, int argc, char **argv)
{
    int state = 0;

    wifi_mgmr_state_get(&state);

    os_printf("%s:wifi state = 0x%x\r\n", __func__, state);
    if(state == WIFI_STATE_UNKNOWN){
        os_printf("wifi current state: WIFI_STATE_UNKNOWN\r\n");
    } else if(state == WIFI_STATE_IDLE) {
        os_printf("wifi current state: WIFI_STATE_IDLE\r\n");
    } else if(state == WIFI_STATE_CONNECTING) {
        os_printf("wifi current state: WIFI_STATE_CONNECTING\r\n");
    } else if(state == WIFI_STATE_CONNECTED_IP_GETTING) {
        os_printf("wifi current state: WIFI_STATE_CONNECTED_IP_GETTING\r\n");
    } else if(state == WIFI_STATE_CONNECTED_IP_GOT) {
        os_printf("wifi current state: WIFI_STATE_CONNECTED_IP_GOT\r\n");
    } else if(state == WIFI_STATE_DISCONNECT) {
        os_printf("wifi current state: WIFI_STATE_DISCONNECT\r\n");
    } else if(state == WIFI_STATE_WITH_AP_IDLE) {
        os_printf("wifi current state: WIFI_STATE_WITH_AP_IDLE\r\n");
    } else if(state == WIFI_STATE_WITH_AP_CONNECTING) {
        os_printf("wifi current state: WIFI_STATE_WITH_AP_CONNECTING\r\n");
    } else if(state == WIFI_STATE_WITH_AP_CONNECTED_IP_GETTING) {
        os_printf("wifi current state: WIFI_STATE_WITH_AP_CONNECTED_IP_GETTING\r\n");
    } else if(state == WIFI_STATE_WITH_AP_CONNECTED_IP_GOT) {
        os_printf("wifi current state: WIFI_STATE_WITH_AP_CONNECTED_IP_GOT\r\n");
    } else if(state == WIFI_STATE_WITH_AP_DISCONNECT) {
        os_printf("wifi current state: WIFI_STATE_WITH_AP_DISCONNECT\r\n");
    } else if(state == WIFI_STATE_IFDOWN) {
        os_printf("wifi current state: WIFI_STATE_IFDOWN\r\n");
    } else if(state == WIFI_STATE_SNIFFER) {
        os_printf("wifi current state: WIFI_STATE_SNIFFER\r\n");
    } else if(state == WIFI_STATE_PSK_ERROR) {
        os_printf("wifi current state: WIFI_STATE_PSK_ERROR\r\n");
    } else if(state == WIFI_STATE_NO_AP_FOUND) {
        os_printf("wifi current state: WIFI_STATE_NO_AP_FOUND\r\n");
    } else {
        os_printf("wifi current state: invalid\r\n");
    }
}

static void wifi_disable_autoreconnect_cmd(char *buf, int len, int argc, char **argv)
{
    wifi_mgmr_sta_autoconnect_disable();
}

static void wifi_enable_autoreconnect_cmd(char *buf, int len, int argc, char **argv)
{
    wifi_mgmr_sta_autoconnect_enable();
}

static void wifi_rc_fixed_enable(char *buf, int len, int argc, char **argv)
{
    uint8_t mode = 0;
    uint8_t mcs = 0;
    uint8_t gi = 0;
    uint16_t rc = 0x0000; //format mode is HT_MF only

    if (argc != 4) {
        os_printf("rc_fix_en [b/g/n] [MCS] [GI]");
        return;
    }
    mode = atoi(argv[1]);
    mcs = atoi(argv[2]);
    gi = atoi(argv[3]);

    os_printf("wifi set mode:%s, mcs:%d, gi:%d\r\n", (mode == 1?"n mode":"b/g mdoe"), mcs, gi);

    if (mode == 1) {
        rc |= mode << 12 | gi << 9 | mcs;
    } else if(mode == 0) {
        rc |= (1 << 9) | (1 << 10) | mcs;
    }

    os_printf("wifi rc:0x%x\r\n", rc);

    wifi_mgmr_rate_config(rc);
}

static void wifi_rc_fixed_disable(char *buf, int len, int argc, char **argv)
{
    uint16_t rc = 0xFFFF;

    wifi_mgmr_rate_config(rc);
}

#if 0
static void wifi_capcode_update(char *buf, int len, int argc, char **argv)
{
    uint8_t cap_in, cap_out;

    if (argc == 1) {
        bl60x_fw_xtal_capcode_get(&cap_in, &cap_out);
        printf("[RF] [CAP] Dump capcode in:out %u:%u\r\n", cap_in, cap_out);
        return;
    }
    if (argc != 3) {
        printf("%s [capcode_in] [capcode_out]\r\n", argv[0]);
        return;
    }

    bl60x_fw_xtal_capcode_get(&cap_in, &cap_out);
    printf("[RF] [CAP] Dump capcode in:out %u:%u\r\n", cap_in, cap_out);
    cap_in = atoi(argv[1]);
    cap_out = atoi(argv[2]);
    printf("[RF] [CAP] Updating capcode to in:out %u:%u\r\n", cap_in, cap_out);
    bl60x_fw_xtal_capcode_update(cap_in, cap_out);
    bl60x_fw_xtal_capcode_get(&cap_in, &cap_out);
    printf("[RF] [CAP] Dump Again capcode in:out %u:%u\r\n", cap_in, cap_out);
}
#endif

static void wifi_denoise_enable_cmd(char *buf, int len, int argc, char **argv)
{
    wifi_mgmr_api_denoise_enable();
}

static void wifi_denoise_disable_cmd(char *buf, int len, int argc, char **argv)
{
    wifi_mgmr_api_denoise_disable();
}

static void wifi_power_saving_on_cmd(char *buf, int len, int argc, char **argv)
{
    wifi_mgmr_sta_powersaving(2);
}

static void wifi_power_saving_off_cmd(char *buf, int len, int argc, char **argv)
{
    wifi_mgmr_sta_powersaving(0);
}

static void sniffer_cb(void *env, uint8_t *pkt, int len)
{
    static unsigned int sniffer_counter, sniffer_last;
    static unsigned int last_tick;

    (void)sniffer_last;
    (void)sniffer_counter;

    sniffer_counter++;
    if ((int)bl_os_tick_get() - (int)last_tick > 10 * 1000) {
        blog_info("[SNIFFER] PKT Number is %d\r\n",
                (int)sniffer_counter - (int)sniffer_last
        );
        last_tick = bl_os_tick_get();
        sniffer_last = sniffer_counter;
    }
}

static void wifi_mon_cmd(char *buf, int len, int argc, char **argv)
{
    if (argc > 1) {
        blog_debug("Enable Sniffer Mode\r\n");
        wifi_mgmr_sniffer_enable();
    } else {
        blog_debug("Register Sniffer cb\r\n");
        wifi_mgmr_sniffer_register(NULL, sniffer_cb);
    }
}

static void wifi_sniffer_on_cmd(char *buf, int len, int argc, char **argv)
{
    wifi_mgmr_sniffer_enable();
    wifi_mgmr_sniffer_register(NULL, sniffer_cb);
}

static void wifi_sniffer_off_cmd(char *buf, int len, int argc, char **argv)
{
    wifi_mgmr_sniffer_disable();
    wifi_mgmr_sniffer_unregister(NULL);
}

static void cmd_wifi_ap_start(char *buf, int len, int argc, char **argv)
{
    uint8_t mac[6];
    uint8_t hidden_ssid = 0;
    char ssid_name[32];
    int channel;
    wifi_interface_t wifi_interface;

    memset(mac, 0, sizeof(mac));
    bl_wifi_mac_addr_get(mac);
    memset(ssid_name, 0, sizeof(ssid_name));
    snprintf(ssid_name, sizeof(ssid_name), "BL60X_uAP_%02X%02X%02X", mac[3], mac[4], mac[5]);
    ssid_name[sizeof(ssid_name) - 1] = '\0';

    wifi_interface = wifi_mgmr_ap_enable();
    if (1 == argc) {
        /*no password when only one param*/
        wifi_mgmr_ap_start(wifi_interface, ssid_name, hidden_ssid, NULL, 1);
    } else {
        /*hardcode password*/
        if (3 == argc) {
            hidden_ssid = 1;
        }
        channel = atoi(argv[1]);
        if (channel <=0 || channel > 11) {
            return;
        }
        wifi_mgmr_ap_start(wifi_interface, ssid_name, hidden_ssid, "12345678", channel);
    }
}

static void cmd_wifi_ap_stop(char *buf, int len, int argc, char **argv)
{
    wifi_mgmr_ap_stop(NULL);
    os_printf("--->>> cmd_wifi_ap_stop\r\n");
}

static void cmd_wifi_ap_conf_max_sta(char *buf, int len, int argc, char **argv)
{
    int max_sta_supported;

    if (2 != argc) {
        os_printf("Usage: wifi_ap_max_sta [num]\r\n");
        return;
    }

    max_sta_supported = atoi(argv[1]);
    os_printf("Conf Max Sta to %d\r\n", max_sta_supported);

    wifi_mgmr_conf_max_sta(max_sta_supported);
}

static void cmd_wifi_dump(char *buf, int len, int argc, char **argv)
{
    if (argc > 1) {
        puts("[CLI] Dump statistic use forced mode\r\n");
        os_critical_enter();
        bl60x_fw_dump_statistic(1);
        os_critical_exit();
    } else {
        puts("[CLI] Dump statistic use normal mode\r\n");
        os_critical_enter();
        bl60x_fw_dump_statistic(0);
        os_critical_exit();
    }
}

static void cmd_wifi_cfg(char *buf, int len, int argc, char **argv)
{
    int opt;
    uint32_t ops;
    uint32_t task = 0, element = 0, type = 0;
    uint32_t val[1];

    getopt_env_t getopt_env;
    utils_getopt_init(&getopt_env, 0);

    ops = CFG_ELEMENT_TYPE_OPS_UNKNOWN;
    while ((opt = utils_getopt(&getopt_env, argc, argv, ":c:T:e:t:v:")) != -1) {
        switch (opt) {
            case 'c':
                if (0 == strcmp("dump",  getopt_env.optarg)) {
                    ops = CFG_ELEMENT_TYPE_OPS_DUMP_DEBUG;
                } else if (0 == strcmp("set", getopt_env.optarg)) {
                    ops = CFG_ELEMENT_TYPE_OPS_SET;
                } else if (0 == strcmp("get", getopt_env.optarg)) {
                    ops = CFG_ELEMENT_TYPE_OPS_GET;
                } else if (0 == strcmp("reset", getopt_env.optarg)) {
                    ops = CFG_ELEMENT_TYPE_OPS_RESET;
                }
                break;
            case 't':
                task = atoi(getopt_env.optarg);
                break;
            case 'e':
                element = atoi(getopt_env.optarg);
                break;
            case 'T':
                type = atoi(getopt_env.optarg);
                break;
            case 'v':
                val[0] = atoi(getopt_env.optarg);
                break;
            case '?':
                os_printf("%s: unknown option %c\r\n", *argv, getopt_env.optopt);
                return;
        }
    }

    os_printf("Target CFG Element Info, task: %lu, element %lu, type %lu, val %lu\r\n",
        task, element, type, val[0]
    );
    switch (ops) {
        case CFG_ELEMENT_TYPE_OPS_SET:
        {
            os_printf("    OPS: %s\r\n", "set");
            wifi_mgmr_cfg_req(CFG_ELEMENT_TYPE_OPS_SET, task, element, type, sizeof(val), val);
        }
        break;
        case CFG_ELEMENT_TYPE_OPS_GET:
        {
            os_printf("    OPS: %s\r\n", "get");
            wifi_mgmr_cfg_req(CFG_ELEMENT_TYPE_OPS_GET, task, element, type, sizeof(val), val);
        }
        break;
        case CFG_ELEMENT_TYPE_OPS_RESET:
        {
            os_printf("    OPS: %s\r\n", "reset");
            wifi_mgmr_cfg_req(CFG_ELEMENT_TYPE_OPS_RESET, task, element, 0, 0, NULL);
        }
        break;
        case CFG_ELEMENT_TYPE_OPS_DUMP_DEBUG:
        {
            os_printf("    OPS: %s\r\n", "dump");
            wifi_mgmr_cfg_req(CFG_ELEMENT_TYPE_OPS_DUMP_DEBUG, 0, 0, 0, 0, NULL);
        }
        break;
        case CFG_ELEMENT_TYPE_OPS_UNKNOWN:
        {
            os_printf("UNKNOWN OPS\r\n");
        }
        break;
    }
}

static void cmd_wifi_mib(char *buf, int len, int argc, char **argv)
{
void hal_mib_dump();
    hal_mib_dump();
    utils_hexdump(argv[0], 30);
}

static int pkt_counter = 0;
int wifi_mgmr_ext_dump_needed()
{
    if (pkt_counter > 0) {
        pkt_counter--;
        return 1;
    }
    return 0;
}

static void cmd_dump_reset(char *buf, int len, int argc, char **argv)
{
    pkt_counter = 10;
}

void coex_wifi_rf_forece_enable(int enable);
static void cmd_wifi_coex_rf_force_on(char *buf, int len, int argc, char **argv)
{
    coex_wifi_rf_forece_enable(1);
}

static void cmd_wifi_coex_rf_force_off(char *buf, int len, int argc, char **argv)
{
    coex_wifi_rf_forece_enable(0);
}

void coex_wifi_pti_forece_enable(int enable);
static void cmd_wifi_coex_pti_force_on(char *buf, int len, int argc, char **argv)
{
    coex_wifi_pti_forece_enable(1);
}

static void cmd_wifi_coex_pti_force_off(char *buf, int len, int argc, char **argv)
{
    coex_wifi_pti_forece_enable(0);
}

void coex_wifi_pta_forece_enable(int enable);
static void cmd_wifi_coex_pta_force_on(char *buf, int len, int argc, char **argv)
{
    coex_wifi_pta_forece_enable(1);
}

static void cmd_wifi_coex_pta_force_off(char *buf, int len, int argc, char **argv)
{
    coex_wifi_pta_forece_enable(0);
}

static void cmd_wifi_state_get(char *buf, int len, int argc, char **argv)
{
    int state = WIFI_STATE_UNKNOWN;
    wifi_mgmr_state_get(&state);

    switch (state) {
        case WIFI_STATE_UNKNOWN:
            os_printf("wifi state unknown\r\n");
            break;
        case WIFI_STATE_IDLE:
            os_printf("wifi state idle\r\n");
            break;
        case WIFI_STATE_CONNECTING:
            os_printf("wifi state connecting\r\n");
            break;
        case WIFI_STATE_CONNECTED_IP_GETTING:
            os_printf("wifi state connected ip getting\r\n");
            break;
        case WIFI_STATE_CONNECTED_IP_GOT:
            os_printf("wifi state connected ip got\r\n");
            break;
        case WIFI_STATE_DISCONNECT:
            os_printf("wifi state disconnect\r\n");
            break;
        case WIFI_STATE_WITH_AP_IDLE:
            os_printf("wifi state with ap idle\r\n");
            break;
        case WIFI_STATE_WITH_AP_CONNECTING:
            os_printf("wifi state with ap connecting\r\n");
            break;
        case WIFI_STATE_WITH_AP_CONNECTED_IP_GETTING:
            os_printf("wifi state with ap connected ip getting\r\n");
            break;
        case WIFI_STATE_WITH_AP_CONNECTED_IP_GOT:
            os_printf("wifi state with ap connected ip got\r\n");
            break;
        case WIFI_STATE_WITH_AP_DISCONNECT:
            os_printf("wifi state with ap disconnect\r\n");
            break;
        case WIFI_STATE_IFDOWN:
            os_printf("wifi state ifdown\r\n");
            break;
        case WIFI_STATE_SNIFFER:
            os_printf("wifi state sniffer\r\n");
            break;
        case WIFI_STATE_PSK_ERROR:
            os_printf("wifi state psk error\r\n");
            break;
        case WIFI_STATE_NO_AP_FOUND:
            os_printf("wifi state no ap found\r\n");
            break;
        default:
            break;
    }
}

static void cmd_wifi_power_table_update(char *buf, int len, int argc, char **argv)
{
    int8_t power_table_test[38] = {
        18, 18, 18, 18, 18, 18, 18, 18, //power dbm for 11b 1Mbps/2Mbps/5Mbps/11Mbps
        16, 16, 16, 16, 16, 16, 14, 14, //power dbm for 11g 6,9,12,18,24,36,48,54 Mbps
        16, 16, 16, 16, 16, 14, 14, 14, //power dbm for 11n MCS0~MCS7
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //power re-cal for channel 1~14
    };
    //call this API before any other Wi-Fi related APi is called, to make sure every thing is all right
    bl_tpc_update_power_table(power_table_test);
}

// STATIC_CLI_CMD_ATTRIBUTE makes this(these) command(s) static
const static struct cli_command cmds_user[] STATIC_CLI_CMD_ATTRIBUTE = {
        { "rf_dump", "rf dump", cmd_rf_dump},
        { "wifi_ap_bcnint_set", "wifi ap bcnin set", wifi_bcnint_set},
        { "wifi_capcode", "wifi capcode", wifi_capcode_cmd},
        { "wifi_scan", "wifi scan", wifi_scan_cmd},
        { "wifi_scan_filter", "wifi scan", wifi_scan_filter_cmd},
        { "wifi_mon", "wifi monitor", wifi_mon_cmd},
        { "wifi_raw_send", "wifi raw send test", cmd_wifi_raw_send},
        { "wifi_sta_info", "wifi sta info", wifi_sta_ip_info},
        { "wifi_sta_ip_set", "wifi STA IP config [ip] [mask] [gw] [dns1] [dns2]", wifi_sta_ip_set_cmd},
        { "wifi_sta_ip_unset", "wifi STA IP config unset", wifi_sta_ip_unset_cmd},
        { "wifi_sta_disconnect", "wifi station disconnect", wifi_disconnect_cmd},
        { "wifi_sta_connect", "wifi station connect", wifi_connect_cmd},
        { "wifi_sta_get_state", "wifi sta get state", wifi_sta_get_state_cmd},
        { "wifi_sta_autoconnect_enable", "wifi station enable auto reconnect", wifi_enable_autoreconnect_cmd},
        { "wifi_sta_autoconnect_disable", "wifi station disable auto reconnect", wifi_disable_autoreconnect_cmd},
        { "rc_fix_en", "wifi rate control fixed rate enable", wifi_rc_fixed_enable},
        { "rc_fix_dis", "wifi rate control fixed rate diable", wifi_rc_fixed_disable},
        { "wifi_sta_ps_on", "wifi power saving mode ON", wifi_power_saving_on_cmd},
        { "wifi_sta_ps_off", "wifi power saving mode OFF", wifi_power_saving_off_cmd},
        { "wifi_sta_denoise_enable", "wifi denoise", wifi_denoise_enable_cmd},
        { "wifi_sta_denoise_disable", "wifi denoise", wifi_denoise_disable_cmd},
        { "wifi_sniffer_on", "wifi sniffer mode on", wifi_sniffer_on_cmd},
        { "wifi_sniffer_off", "wifi sniffer mode off", wifi_sniffer_off_cmd},
        { "wifi_ap_start", "start Ap mode", cmd_wifi_ap_start},
        { "wifi_ap_stop", "stop Ap mode", cmd_wifi_ap_stop},
        { "wifi_ap_conf_max_sta", "config Ap max sta", cmd_wifi_ap_conf_max_sta},
        { "wifi_dump", "dump fw statistic", cmd_wifi_dump},
        { "wifi_cfg", "wifi cfg cmd", cmd_wifi_cfg},
        { "wifi_mib", "dump mib statistic", cmd_wifi_mib},
        { "wifi_pkt", "wifi dump needed", cmd_dump_reset},
        { "wifi_coex_rf_force_on", "wifi coex RF force on", cmd_wifi_coex_rf_force_on},
        { "wifi_coex_rf_force_off", "wifi coex RF force off", cmd_wifi_coex_rf_force_off},
        { "wifi_coex_pti_force_on", "wifi coex PTI force on", cmd_wifi_coex_pti_force_on},
        { "wifi_coex_pti_force_off", "wifi coex PTI force off", cmd_wifi_coex_pti_force_off},
        { "wifi_coex_pta_force_on", "wifi coex PTA force on", cmd_wifi_coex_pta_force_on},
        { "wifi_coex_pta_force_off", "wifi coex PTA force off", cmd_wifi_coex_pta_force_off},
        { "wifi_sta_list", "get sta list in AP mode", wifi_ap_sta_list_get_cmd},
        { "wifi_sta_del", "delete one sta in AP mode", wifi_ap_sta_delete_cmd},
        { "wifi_edca_dump", "dump EDCA data", wifi_edca_dump_cmd},
        { "wifi_state", "get wifi_state", cmd_wifi_state_get},
        { "wifi_update_power", "Power table test command", cmd_wifi_power_table_update},
};                                                                                   

int wifi_mgmr_cli_init(void)
{
    // static command(s) do NOT need to call aos_cli_register_command(s) to register.
    // However, calling aos_cli_register_command(s) here is OK but is of no effect as cmds_user are included in cmds list.
    // XXX NOTE: Calling this *empty* function is necessary to make cmds_user in this file to be kept in the final link.
    return aos_cli_register_commands(cmds_user, sizeof(cmds_user)/sizeof(cmds_user[0]));          
    return 0;
}
