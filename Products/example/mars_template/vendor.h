/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */
#ifndef __VENDOR_H__
#define __VENDOR_H__

#include <aos/aos.h>

/**
 * @brief set device meta info.
 *
 * @param [in] None.
 * @param [in] None.
 *
 * @return 0:success, -1:failed.
 * @see None.
 */
int set_device_meta_info(void);

/**
 * @brief vendor operation after receiving device_bind event from Cloud.
 *
 * @param [in] None.
 *
 * @return None.
 * @see None.
 */
void vendor_device_bind(void);

/**
 * @brief vendor operation after receiving device_unbind event from Cloud.
 *
 * @param [in] None.
 *
 * @return None.
 * @see None.
 */
void vendor_device_unbind(void);
/**
 * @brief vendor operation after receiving device_reset event from Cloud.
 *
 * @param [in] None.
 *
 * @return None.
 * @see None.
 */
void vendor_device_reset(void);

int vendor_get_product_key(char *product_key, int *len);
int vendor_get_product_secret(char *product_secret, int *len);
int vendor_get_device_name(char *device_name, int *len);
int vendor_get_device_secret(char *device_secret, int *len);
int vendor_get_product_id(uint32_t *pid);

#endif
