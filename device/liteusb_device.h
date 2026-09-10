#ifndef _LITEUSB_DEVICE_H_
#define _LITEUSB_DEVICE_H_

#include "usb_def.h"
#include "liteusb_hal_cfg.h"
#include "liteusb_device_cfg.h"


#define LUSBD_LOW_SPEED   0
#define LUSBD_FULL_SPEED  1
#define LUSBD_HIGH_SPEED  2

#define LUSBD_DIR_OUT  0
#define LUSBD_DIR_IN   1

#define LUSBD_EP_TYPE_CONTROL      0
#define LUSBD_EP_TYPE_ISOCHRONOUS  1
#define LUSBD_EP_TYPE_BULK         2
#define LUSBD_EP_TYPE_INTERRUPT    3


typedef struct
{
    uint8_t used;
    uint8_t class_idx;

    uint8_t if_num;
    uint8_t alt_num;
    uint8_t dir;
    uint8_t ep_num;
    uint8_t type;
    uint16_t max_packet_size;
    uint16_t max_alt_mps;

    uint8_t active;
    uint8_t halt;
    uint8_t need_zlp;
} lusbd_ep_t;

typedef struct
{
    void (*activate)(unsigned int dev_idx, unsigned int is_hs, void *class_data);
    void (*deactivate)(unsigned int dev_idx, void *class_data);
    void (*setup)(unsigned int dev_idx, const usb_setup_data_t *setup_data, void *class_data);
    void (*data_out)(unsigned int dev_idx, unsigned int ep_num, uint8_t *buf, unsigned int len, void *class_data);
    void (*data_in_complete)(unsigned int dev_idx, unsigned int ep_num, void *class_data);
} lusbd_class_cb_t;

#define LUSBD_USER_EVENT_CONNECTED     1
#define LUSBD_USER_EVENT_DISCONNECTED  2
#define LUSBD_USER_EVENT_CONFIGURED    3
#define LUSBD_USER_EVENT_SUSPENDED     4
#define LUSBD_USER_EVENT_RESUMED       5
typedef void (*lusbd_user_cb_t)(unsigned int dev_idx, unsigned int event);

typedef struct
{
    uint8_t dev_state;
    uint8_t suspend;
    uint8_t enum_speed;
    uint8_t cur_cfg_val;
} lusbd_device_info_t;


int lusbd_add_str(const uint8_t *buf, unsigned int len);
int lusbd_set_dev_info(unsigned int dev_idx, uint16_t vid, uint16_t pid, uint16_t ver, unsigned int i_mf, unsigned int i_prod, unsigned int i_sn);
int lusbd_set_cfg_info(unsigned int dev_idx, unsigned int cfg_val, unsigned int is_hs, unsigned int i_cfg, unsigned int is_self_powered, unsigned int max_power);
int lusbd_set_user_cb(unsigned int dev_idx, lusbd_user_cb_t cb);

int lusbd_register_class(unsigned int dev_idx, unsigned int cfg_val, unsigned int is_hs, const lusbd_class_cb_t *cb, void *class_data);
int lusbd_alloc_interface(unsigned int class_idx, unsigned int count, unsigned int *if_num_base);
int lusbd_add_endpoint(unsigned int class_idx, unsigned int if_num, uint8_t alt_num, unsigned int dir, unsigned int ep_num, unsigned int type, unsigned int mps, unsigned int *ep_num_base);
int lusbd_add_descriptor(unsigned int class_idx, const uint8_t *desc, unsigned int len);
void lusbd_print_res_usage(void);

int lusbd_get_device_info(unsigned int dev_idx, lusbd_device_info_t *info);
int lusbd_start_device(unsigned int dev_idx);
int lusbd_stop_device(unsigned int dev_idx);
int lusbd_ep_set_halt(unsigned int dev_idx, unsigned int dir, unsigned int num);
int lusbd_ep_clear_halt(unsigned int dev_idx, unsigned int dir, unsigned int num);
int lusbd_ep_tx(unsigned int dev_idx, unsigned int num, const void *buf, unsigned int len, unsigned int need_zlp);
int lusbd_ep_rx(unsigned int dev_idx, unsigned int num, void *buf, unsigned int len);
int lusbd_control_tx_status(unsigned int dev_idx);
int lusbd_control_rx_status(unsigned int dev_idx);
typedef void (*lusbd_control_out_cb_t)(unsigned int dev_idx, uint8_t *buf, unsigned int len, void *arg, void *class_data);
int lusbd_control_rx(unsigned int dev_idx, void *buf, unsigned int len, lusbd_control_out_cb_t cb, void *arg, void *class_data);

void lusbd_connect_handler(unsigned int dev_idx);
void lusbd_disconnect_handler(unsigned int dev_idx);
void lusbd_suspend_handler(unsigned int dev_idx);
void lusbd_resume_handler(unsigned int dev_idx);
void lusbd_reset_handler(unsigned int dev_idx, unsigned int speed);
void lusbd_setup_handler(unsigned int dev_idx, const uint8_t *setup_data);
void lusbd_data_in_handler(unsigned int dev_idx, unsigned int ep_num);
void lusbd_data_out_handler(unsigned int dev_idx, unsigned int ep_num, uint8_t *buf, unsigned int recv_len);


#endif
