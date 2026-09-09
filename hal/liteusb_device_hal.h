#ifndef _LITEUSB_DEVICE_HAL_H_
#define _LITEUSB_DEVICE_HAL_H_

#include <stdint.h>
#include "liteusb_device.h"


int lusbd_hal_start_device(unsigned int dev_idx);
int lusbd_hal_stop_device(unsigned int dev_idx);
int lusbd_hal_ep_open(unsigned int dev_idx, unsigned int dir, unsigned int num, unsigned int mps, unsigned int type);
int lusbd_hal_ep_set_halt(unsigned int dev_idx, unsigned int dir, unsigned int num);
int lusbd_hal_ep_clear_halt(unsigned int dev_idx, unsigned int dir, unsigned int num);
int lusbd_hal_ep_tx(unsigned int dev_idx, unsigned int num, const void *buf, unsigned int len);
int lusbd_hal_ep_rx(unsigned int dev_idx, unsigned int num, void *buf, unsigned int len);
int lusbd_hal_set_address(unsigned int dev_idx, unsigned int addr);
int lusbd_hal_init_eps(unsigned int dev_idx, const lusbd_ep_t *out_ep_list, unsigned int out_ep_count, const lusbd_ep_t *in_ep_list, unsigned int in_ep_count);
int lusbd_hal_set_remote_wakeup(unsigned int dev_idx, unsigned int enable);
int lusbd_hal_set_test_mode(unsigned int dev_idx, unsigned int test_mode);


#endif
