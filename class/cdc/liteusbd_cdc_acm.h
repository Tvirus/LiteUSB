#ifndef _LITEUSBD_CDC_ACM_H_
#define _LITEUSBD_CDC_ACM_H_

#include "liteusb_device.h"


#define CDCACM_RX_MPS_FS  LUSBD_MAX_PACKET_SIZE_FS_BULK
#define CDCACM_RX_MPS_HS  USB_EP_MAX_PACKET_SIZE_HS_BULK


#define SERIAL_CONFIG_STOP_BIT_1    0
#define SERIAL_CONFIG_STOP_BIT_1_5  1
#define SERIAL_CONFIG_STOP_BIT_2    2
#define SERIAL_CONFIG_PARITY_TYPE_NONE   0
#define SERIAL_CONFIG_PARITY_TYPE_ODD    1
#define SERIAL_CONFIG_PARITY_TYPE_EVEN   2
#define SERIAL_CONFIG_PARITY_TYPE_MARK   3
#define SERIAL_CONFIG_PARITY_TYPE_SPACE  4
typedef struct __attribute__((packed))
{
    uint32_t baud_rate;
    uint8_t stop_bits;
    uint8_t parity_type;
    uint8_t data_bits;
} cdcacm_serial_config_t;

typedef struct
{
    void (*recv)(unsigned int cdc_idx, const uint8_t *buf, unsigned int len);
    void (*send_complete)(unsigned int cdc_idx);
    int (*set_serial_config)(unsigned int cdc_idx, const cdcacm_serial_config_t *config);
    int (*get_serial_config)(unsigned int cdc_idx, cdcacm_serial_config_t *config);
} cdcacmd_cb_t;


int cdcacmd_add_class(unsigned int dev_idx, unsigned int cfg_val, unsigned int i_func, unsigned int i_if_comm, unsigned int i_if_data, cdcacmd_cb_t *cb);
int cdcacmd_recv(unsigned int cdc_idx, uint8_t *buf, unsigned int len);
int cdcacmd_send(unsigned int cdc_idx, const void *buf, unsigned int len);


#endif
