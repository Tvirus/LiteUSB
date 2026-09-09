#include "liteusbd_cdc_acm.h"
#include "cdc_def.h"
#include <string.h>




#define CDCACM_COMM_IN_EP_NUM  1
#define CDCACM_DATA_IN_EP_NUM  2
#ifdef USB_EP_DIR_INDEPENDENT
#define CDCACM_DATA_OUT_EP_NUM  1
#else
#define CDCACM_DATA_OUT_EP_NUM  2
#endif


typedef struct __attribute__((packed))
{
    uint8_t bFunctionLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    union
    {
        uint8_t bmCapabilities;
        struct
        {
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
            uint8_t bmCapabilities_rsv:4;
            uint8_t bmCapabilities_spt_net_conn_notify:1;
            uint8_t bmCapabilities_spt_sendbreak_req:1;
            uint8_t bmCapabilities_spt_line_req:1;
            uint8_t bmCapabilities_spt_commfeature_req:1;
#else
            uint8_t bmCapabilities_spt_commfeature_req:1;
            uint8_t bmCapabilities_spt_line_req:1;
            uint8_t bmCapabilities_spt_sendbreak_req:1;
            uint8_t bmCapabilities_spt_net_conn_notify:1;
            uint8_t bmCapabilities_rsv:4;
#endif
        };
    };
} cdcacm_acm_desc_t;

typedef struct __attribute__((packed))
{
    uint8_t bFunctionLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    union
    {
        uint8_t bmCapabilities;
        struct
        {
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
            uint8_t bmCapabilities_rsv:6;
            uint8_t bmCapabilities_call_mgmt_over_data:1;
            uint8_t bmCapabilities_handles_call_mgmt:1;
#else
            uint8_t bmCapabilities_handles_call_mgmt:1;
            uint8_t bmCapabilities_call_mgmt_over_data:1;
            uint8_t bmCapabilities_rsv:6;
#endif
        };
    };
    uint8_t bDataInterface;
} cdcacm_cm_desc_t;

typedef struct __attribute__((packed))
{
    uint8_t bFunctionLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint8_t bControlInterface;
    uint8_t bSubordinateInterface0;
} cdcacm_union_desc_t;

#define CDCACM_LINE_CODING_FORMAT_STOP_BIT_1    0
#define CDCACM_LINE_CODING_FORMAT_STOP_BIT_1_5  1
#define CDCACM_LINE_CODING_FORMAT_STOP_BIT_2    2
#define CDCACM_LINE_CODING_PARITY_TYPE_NONE   0
#define CDCACM_LINE_CODING_PARITY_TYPE_ODD    1
#define CDCACM_LINE_CODING_PARITY_TYPE_EVEN   2
#define CDCACM_LINE_CODING_PARITY_TYPE_MARK   3
#define CDCACM_LINE_CODING_PARITY_TYPE_SPACE  4
typedef struct __attribute__((packed))
{
    uint8_t dwDTERate_0;
    uint8_t dwDTERate_1;
    uint8_t dwDTERate_2;
    uint8_t dwDTERate_3;

    uint8_t bCharFormat;
    uint8_t bParityType;
    uint8_t bDataBits;
} cdcacm_line_coding_t;

typedef struct
{
    uint8_t dev_idx;
    uint8_t cfg_val;

    uint8_t out_ep_num_base;
    uint8_t in_ep_num_base;
#if LUSBD_MAX_SPEED == LUSBD_HIGH_SPEED
    uint8_t hs_out_ep_num_base;
    uint8_t hs_in_ep_num_base;
#endif
    uint8_t active;
    uint8_t cur_out_ep_num_base;
    uint8_t cur_in_ep_num_base;

    cdcacmd_cb_t *cb;
    cdcacm_line_coding_t line_coding __attribute__((aligned(4)));

    union __attribute__((aligned(4)))
    {
        cdcacm_line_coding_t line_coding;
    } rx_buf;
} cdcacmd_t;


const static usb_interface_association_descriptor_t cdcacm_iad_desc =
{
    sizeof(usb_interface_association_descriptor_t),
    USB_DESC_TYPE_INTERFACE_ASSOCIATION,
    0,                             /* bFirstInterface */
    2,                             /* bInterfaceCount */
    USB_INTERFACE_CLASS_COMM,      /* bFunctionClass */
    USB_COMM_IF_SUBCLASS_ACM,      /* bFunctionSubClass */
    USB_COMM_IF_PROTOCOL_AT_V250,  /* bFunctionProtocol */
    0                              /* iFunction */
};

const static usb_interface_descriptor_t cdcacm_comm_if_desc =
{
    sizeof(usb_interface_descriptor_t),
    USB_DESC_TYPE_INTERFACE,
    0,                             /* bInterfaceNumber */
    0,                             /* bAlternateSetting */
    1,                             /* bNumEndpoints */
    USB_INTERFACE_CLASS_COMM,      /* bInterfaceClass */
    USB_COMM_IF_SUBCLASS_ACM,      /* bInterfaceSubClass */
    USB_COMM_IF_PROTOCOL_AT_V250,  /* bInterfaceProtocol */
    0x00                           /* iInterface */
};

const static comm_if_header_desc_t cdcacm_header_desc =
{
    sizeof(comm_if_header_desc_t),
    USB_DESC_TYPE_CS_INTERFACE,
    COMM_IF_FUNC_DESC_ID_HEADER,  /* bDescriptorSubtype; */
    0x10,                         /* bcdCDC_l; */
    0x01                          /* bcdCDC_h; */
};

const static cdcacm_acm_desc_t cdcacm_acm_desc =
{
    .bFunctionLength = sizeof(cdcacm_acm_desc_t),
    .bDescriptorType = USB_DESC_TYPE_CS_INTERFACE,
    .bDescriptorSubtype = COMM_IF_FUNC_DESC_ID_ACM,

    .bmCapabilities_rsv = 0,
    .bmCapabilities_spt_net_conn_notify = 0,
    .bmCapabilities_spt_sendbreak_req = 0,
    .bmCapabilities_spt_line_req = 1,
    .bmCapabilities_spt_commfeature_req = 0
};

const static cdcacm_union_desc_t cdcacm_union_desc =
{
    sizeof(cdcacm_union_desc_t),
    USB_DESC_TYPE_CS_INTERFACE,
    COMM_IF_FUNC_DESC_ID_UNION,  /* bDescriptorSubtype; */
    0,                           /* bControlInterface */
    1                            /* bSubordinateInterface0 */
};

const static cdcacm_cm_desc_t cdcacm_cm_desc =
{
    .bFunctionLength = sizeof(cdcacm_cm_desc_t),
    .bDescriptorType = USB_DESC_TYPE_CS_INTERFACE,
    .bDescriptorSubtype = COMM_IF_FUNC_DESC_ID_CM,

    .bmCapabilities_rsv = 0,
    .bmCapabilities_call_mgmt_over_data = 0,
    .bmCapabilities_handles_call_mgmt = 0,

    .bDataInterface = 1
};

const static usb_endpoint_descriptor_t cdcacm_cmd_ep_desc =
{
    .bLength = sizeof(usb_endpoint_descriptor_t),
    .bDescriptorType = USB_DESC_TYPE_ENDPOINT,

    .bEndpointAddress_direction = 1,
    .bEndpointAddress_rsv = 0,
    .bEndpointAddress_number = CDCACM_COMM_IN_EP_NUM,

    .bmAttributes_rsv = 0,
    .bmAttributes_usage_type = USB_EP_DESC_USAGE_TYPE_DATA,
    .bmAttributes_synchronization_type = USB_EP_DESC_SYNC_TYPE_NOSYNC,
    .bmAttributes_transfer_type = USB_EP_DESC_TRANSFER_TYPE_INTERRUPT,

    .wMaxPacketSize_l = 8,
    .wMaxPacketSize_h = 0,
    .bInterval = 16
};

const static usb_interface_descriptor_t cdcacm_data_if_desc =
{
    sizeof(usb_interface_descriptor_t),
    USB_DESC_TYPE_INTERFACE, 
    1,                         /* bInterfaceNumber */
    0,                         /* bAlternateSetting */
    2,                         /* bNumEndpoints */
    USB_INTERFACE_CLASS_DATA,  /* bInterfaceClass */
    0,                         /* bInterfaceSubClass */
    0,                         /* bInterfaceProtocol */
    0                          /* iInterface */
};

const static usb_endpoint_descriptor_t cdcacm_data_out_ep_desc =
{
    .bLength = sizeof(usb_endpoint_descriptor_t),
    .bDescriptorType = USB_DESC_TYPE_ENDPOINT,

    .bEndpointAddress_direction = 0,
    .bEndpointAddress_rsv = 0,
    .bEndpointAddress_number = CDCACM_DATA_OUT_EP_NUM,

    .bmAttributes_rsv = 0,
    .bmAttributes_usage_type = USB_EP_DESC_USAGE_TYPE_DATA,
    .bmAttributes_synchronization_type = USB_EP_DESC_SYNC_TYPE_NOSYNC,
    .bmAttributes_transfer_type = USB_EP_DESC_TRANSFER_TYPE_BULK,

    .wMaxPacketSize_l = LUSBD_MAX_PACKET_SIZE_FS_BULK,
    .wMaxPacketSize_h = 0,
    .bInterval = 0
};

const static usb_endpoint_descriptor_t cdcacm_data_in_ep_desc =
{
    .bLength = sizeof(usb_endpoint_descriptor_t),
    .bDescriptorType = USB_DESC_TYPE_ENDPOINT,

    .bEndpointAddress_direction = 1,
    .bEndpointAddress_rsv = 0,
    .bEndpointAddress_number = CDCACM_DATA_IN_EP_NUM,

    .bmAttributes_rsv = 0,
    .bmAttributes_usage_type = USB_EP_DESC_USAGE_TYPE_DATA,
    .bmAttributes_synchronization_type = USB_EP_DESC_SYNC_TYPE_NOSYNC,
    .bmAttributes_transfer_type = USB_EP_DESC_TRANSFER_TYPE_BULK,

    .wMaxPacketSize_l = LUSBD_MAX_PACKET_SIZE_FS_BULK,
    .wMaxPacketSize_h = 0,
    .bInterval = 0
};

#ifndef CDCACM_LIST_LEN
#define CDCACM_LIST_LEN  2
#endif
static cdcacmd_t cdcacmd_list[CDCACM_LIST_LEN] = {0};
unsigned int cdcacmd_count = 0;


static void acm_control_out_cb(unsigned int dev_idx, uint8_t *buf, unsigned int len, void *arg, void *class_data)
{
    cdcacm_line_coding_t *line_coding = (cdcacm_line_coding_t *)buf;
    unsigned int rate;
    cdcacmd_t *cdcacmd;
    cdcacm_serial_config_t serial_config;

    if (COMM_IF_REQ_SET_LINE_CODING == (unsigned int)arg)
    {
        if (sizeof(cdcacm_line_coding_t) != len)
        {
            lusbd_ep_set_halt(dev_idx, 0, 0);
            LUSBD_INFO("CDC-ACM(%u) recv SET_LINE_CODING data invalid, data_len(%u), ep0 halted", (unsigned int)class_data, len);
            return;
        }
        rate =  ((unsigned int)line_coding->dwDTERate_3 << 24)
              | ((unsigned int)line_coding->dwDTERate_2 << 16)
              | ((unsigned int)line_coding->dwDTERate_1 << 8)
              |  (unsigned int)line_coding->dwDTERate_0;
        LUSBD_DEBUG("CDC-ACM(%u) recv SET_LINE_CODING data, rate:%u stop:%u parity:%u bits:%u",
                    (unsigned int)class_data, rate, line_coding->bCharFormat, line_coding->bParityType, line_coding->bDataBits);

        cdcacmd = &cdcacmd_list[(unsigned int)class_data];
        if (cdcacmd->cb && cdcacmd->cb->set_serial_config)
        {
            serial_config.baud_rate = rate;
            serial_config.stop_bits = line_coding->bCharFormat;
            serial_config.parity_type = line_coding->bParityType;
            serial_config.data_bits = line_coding->bDataBits;
            if (cdcacmd->cb->set_serial_config((unsigned int)class_data, &serial_config))
            {
                lusbd_ep_set_halt(dev_idx, 0, 0);
                LUSBD_INFO("CDC-ACM(%u) SET_LINE_CODING failed, rate:%u stop:%u parity:%u bits:%u, ep0 halted",
                           (unsigned int)class_data, rate, line_coding->bCharFormat, line_coding->bParityType, line_coding->bDataBits);
                return;
            }
        }
        cdcacmd->line_coding = *line_coding;
    }
    lusbd_control_tx_status(dev_idx);
    return;
}

static void acm_activate_cb(unsigned int dev_idx, unsigned int is_hs, void *class_data)
{
    cdcacmd_t *cdcacmd;

    cdcacmd = &cdcacmd_list[(unsigned int)class_data];
    cdcacmd->active = 1;
#if LUSBD_MAX_SPEED == LUSBD_HIGH_SPEED
    if (is_hs)
    {
        cdcacmd->cur_out_ep_num_base = cdcacmd->hs_out_ep_num_base;
        cdcacmd->cur_in_ep_num_base = cdcacmd->hs_in_ep_num_base;
    }
    else
#endif
    {
        cdcacmd->cur_out_ep_num_base = cdcacmd->out_ep_num_base;
        cdcacmd->cur_in_ep_num_base = cdcacmd->in_ep_num_base;
    }

    LUSBD_DEBUG("CDC-ACM(%u) activated", (unsigned int)class_data);
    return;
}

static void acm_deactivate_cb(unsigned int dev_idx, void *class_data)
{
    cdcacmd_t *cdcacmd;

    cdcacmd = &cdcacmd_list[(unsigned int)class_data];
    if (cdcacmd->active)
    {
        cdcacmd->active = 0;
        LUSBD_DEBUG("CDC-ACM(%u) deactivated", (unsigned int)class_data);
    }
    return;
}

static void acm_setup_cb(unsigned int dev_idx, const usb_setup_data_t *setup_data, void *class_data)
{
    cdcacmd_t *cdcacmd;
    unsigned int data_len;
    cdcacm_serial_config_t serial_config;


    cdcacmd = &cdcacmd_list[(unsigned int)class_data];
    data_len = (setup_data->wLength_h << 8) | setup_data->wLength_l;

    if (COMM_IF_REQ_SEND_ENCAPSULATED_COMMAND == setup_data->bRequest)
    {
        goto HALT;
    }
    else if (COMM_IF_REQ_GET_ENCAPSULATED_RESPONSE == setup_data->bRequest)
    {
        goto HALT;
    }
    else if (COMM_IF_REQ_SET_COMM_FEATURE == setup_data->bRequest)
    {
        goto HALT;
    }
    else if (COMM_IF_REQ_GET_COMM_FEATURE == setup_data->bRequest)
    {
        goto HALT;
    }
    else if (COMM_IF_REQ_CLEAR_COMM_FEATURE == setup_data->bRequest)
    {
        goto HALT;
    }
    else if (COMM_IF_REQ_SET_LINE_CODING == setup_data->bRequest)
    {
        if (0x21 != setup_data->bmRequestType)
            goto HALT;
        if (sizeof(cdcacmd->rx_buf.line_coding) != data_len)
        {
            LUSBD_INFO("CDC-ACM(%u) recv SET_LINE_CODING invalid, data_len:%u, ep0 halted", (unsigned int)class_data, data_len);
            goto HALT;
        }
        lusbd_control_rx(dev_idx, &cdcacmd->rx_buf.line_coding, data_len, acm_control_out_cb, (void *)COMM_IF_REQ_SET_LINE_CODING, class_data);
        LUSBD_DEBUG("CDC-ACM(%u) recv SET_LINE_CODING", (unsigned int)class_data);
    }
    else if (COMM_IF_REQ_GET_LINE_CODING == setup_data->bRequest)
    {
        if (0xa1 != setup_data->bmRequestType)
            goto HALT;

        if (   cdcacmd->cb
            && cdcacmd->cb->get_serial_config
            && (0 == cdcacmd->cb->get_serial_config((unsigned int)class_data, &serial_config)))
        {
            cdcacmd->line_coding.dwDTERate_0 = serial_config.baud_rate & 0xff;
            cdcacmd->line_coding.dwDTERate_1 = (serial_config.baud_rate >> 8) & 0xff;
            cdcacmd->line_coding.dwDTERate_2 = (serial_config.baud_rate >> 16) & 0xff;
            cdcacmd->line_coding.dwDTERate_3 = (serial_config.baud_rate >> 24) & 0xff;
            cdcacmd->line_coding.bCharFormat = serial_config.stop_bits;
            cdcacmd->line_coding.bParityType = serial_config.parity_type;
            cdcacmd->line_coding.bDataBits = serial_config.data_bits;
        }

        if (sizeof(cdcacmd->line_coding) < data_len)
            lusbd_ep_tx(dev_idx, 0, (uint8_t *)&cdcacmd->line_coding, sizeof(cdcacmd->line_coding), 1);
        else
            lusbd_ep_tx(dev_idx, 0, (uint8_t *)&cdcacmd->line_coding, data_len, 0);
        lusbd_control_rx_status(dev_idx);
        LUSBD_DEBUG("CDC-ACM(%u) recv GET_LINE_CODING", (unsigned int)class_data);
    }
    else if (COMM_IF_REQ_SET_CONTROL_LINE_STATE == setup_data->bRequest)
    {
        if (0x21 != setup_data->bmRequestType)
            goto HALT;
        lusbd_control_tx_status(dev_idx);
        LUSBD_DEBUG("CDC-ACM(%u) recv SET_CONTROL_LINE_STATE, value:0x%02x%02x", (unsigned int)class_data, setup_data->wValue_h, setup_data->wValue_l);
    }
    else if (COMM_IF_REQ_SEND_BREAK == setup_data->bRequest)
    {
        goto HALT;
    }
    else
    {
        LUSBD_INFO("CDC-ACM(%u) setup invalid, bRequest:%u, ep0 halted", (unsigned int)class_data, setup_data->bRequest);
        goto HALT;
    }

    return;

HALT:
    lusbd_ep_set_halt(dev_idx, 0, 0);
    return;
}

static void acm_data_out_cb(unsigned int dev_idx, uint8_t *buf, unsigned int len, void *class_data)
{
    cdcacmd_t *cdcacmd;

    cdcacmd = &cdcacmd_list[(unsigned int)class_data];
    if (cdcacmd->cb && cdcacmd->cb->recv)
        cdcacmd->cb->recv((unsigned int)class_data, buf, len);
    return;
}

static void acm_data_in_complete_cb(unsigned int dev_idx, void *class_data)
{
    cdcacmd_t *cdcacmd;

    cdcacmd = &cdcacmd_list[(unsigned int)class_data];
    if (cdcacmd->cb && cdcacmd->cb->send_complete)
        cdcacmd->cb->send_complete((unsigned int)class_data);
    return;
}

const static lusbd_class_cb_t cdcacmd_class_cb =
{
    acm_activate_cb,
    acm_deactivate_cb,
    acm_setup_cb,
    acm_data_out_cb,
    acm_data_in_complete_cb
};

int cdcacmd_add_class(unsigned int dev_idx, unsigned int cfg_val, unsigned int i_func, unsigned int i_if_comm, unsigned int i_if_data, cdcacmd_cb_t *cb)
{
    cdcacmd_t *cdcacmd;
    int class_idx;
    unsigned int if_num_base;
    unsigned int in_ep_num_base;
    unsigned int out_ep_num_base;
#if LUSBD_MAX_SPEED == LUSBD_HIGH_SPEED
    unsigned int hs_in_ep_num_base;
    unsigned int hs_out_ep_num_base;
#endif
    uint8_t buf[32];
    usb_interface_association_descriptor_t *iad_desc;
    usb_interface_descriptor_t *if_desc;
    cdcacm_union_desc_t *union_desc;
    cdcacm_cm_desc_t *cm_desc;
    usb_endpoint_descriptor_t *ep_desc;
    cdcacm_serial_config_t serial_config;


    if (NULL == cb)
        return -1;

#if LUSBD_MAX_SPEED == LUSBD_LOW_SPEED
    LUSBD_ERROR("CDC-ACM add class failed, can not support low speed");
    return -1;
#endif

    if (CDCACM_LIST_LEN <= cdcacmd_count)
    {
        LUSBD_ERROR("CDC-ACM add class failed, increase CDCACM_LIST_LEN(%u)", CDCACM_LIST_LEN);
        return -1;
    }
    cdcacmd = &cdcacmd_list[cdcacmd_count];
    LUSBD_DEBUG("CDC-ACM add class, index:%u", cdcacmd_count);

    class_idx = lusbd_register_class(dev_idx, cfg_val, 0, &cdcacmd_class_cb, (void *)cdcacmd_count);
    if (0 > class_idx)
        return -1;
    if (lusbd_alloc_interface(class_idx, 2, &if_num_base))
        return -1;
    if (lusbd_add_endpoint(class_idx, 0, 0, LUSBD_DIR_IN, 1, LUSBD_EP_TYPE_INTERRUPT, 8, &in_ep_num_base))
        return -1;
#ifdef USB_EP_DIR_INDEPENDENT
    if (lusbd_add_endpoint(class_idx, 1, 0, LUSBD_DIR_OUT, 1, LUSBD_EP_TYPE_BULK, LUSBD_MAX_PACKET_SIZE_FS_BULK, &out_ep_num_base))
#else
    if (lusbd_add_endpoint(class_idx, 1, 0, LUSBD_DIR_OUT, 2, LUSBD_EP_TYPE_BULK, LUSBD_MAX_PACKET_SIZE_FS_BULK, &out_ep_num_base))
#endif
        return -1;
    if (lusbd_add_endpoint(class_idx, 1, 0, LUSBD_DIR_IN, 2, LUSBD_EP_TYPE_BULK, LUSBD_MAX_PACKET_SIZE_FS_BULK, NULL))
        return -1;

    memcpy(buf, &cdcacm_iad_desc, sizeof(cdcacm_iad_desc));
    iad_desc = (usb_interface_association_descriptor_t *)buf;
    iad_desc->bFirstInterface += if_num_base;
    iad_desc->iFunction = i_func;
    if (lusbd_add_descriptor(class_idx, (uint8_t *)iad_desc, sizeof(cdcacm_iad_desc)))
        return -1;

    memcpy(buf, &cdcacm_comm_if_desc, sizeof(cdcacm_comm_if_desc));
    if_desc = (usb_interface_descriptor_t *)buf;
    if_desc->bInterfaceNumber += if_num_base;
    if_desc->iInterface = i_if_comm;
    if (lusbd_add_descriptor(class_idx, (uint8_t *)if_desc, sizeof(cdcacm_comm_if_desc)))
        return -1;

    if (lusbd_add_descriptor(class_idx, (uint8_t *)&cdcacm_header_desc, sizeof(cdcacm_header_desc)))
        return -1;
    if (lusbd_add_descriptor(class_idx, (uint8_t *)&cdcacm_acm_desc, sizeof(cdcacm_acm_desc)))
        return -1;

    memcpy(buf, &cdcacm_union_desc, sizeof(cdcacm_union_desc));
    union_desc = (cdcacm_union_desc_t *)buf;
    union_desc->bControlInterface += if_num_base;
    union_desc->bSubordinateInterface0 += if_num_base;
    if (lusbd_add_descriptor(class_idx, (uint8_t *)union_desc, sizeof(cdcacm_union_desc)))
        return -1;

    memcpy(buf, &cdcacm_cm_desc, sizeof(cdcacm_cm_desc));
    cm_desc = (cdcacm_cm_desc_t *)buf;
    cm_desc->bDataInterface += if_num_base;
    if (lusbd_add_descriptor(class_idx, (uint8_t *)cm_desc, sizeof(cdcacm_cm_desc)))
        return -1;

    memcpy(buf, &cdcacm_cmd_ep_desc, sizeof(cdcacm_cmd_ep_desc));
    ep_desc = (usb_endpoint_descriptor_t *)buf;
    ep_desc->bEndpointAddress_number += in_ep_num_base;
    if (lusbd_add_descriptor(class_idx, (uint8_t *)ep_desc, sizeof(cdcacm_cmd_ep_desc)))
        return -1;

    memcpy(buf, &cdcacm_data_if_desc, sizeof(cdcacm_data_if_desc));
    if_desc = (usb_interface_descriptor_t *)buf;
    if_desc->bInterfaceNumber += if_num_base;
    if_desc->iInterface = i_if_data;
    if (lusbd_add_descriptor(class_idx, (uint8_t *)if_desc, sizeof(cdcacm_data_if_desc)))
        return -1;

    memcpy(buf, &cdcacm_data_out_ep_desc, sizeof(cdcacm_data_out_ep_desc));
    ep_desc = (usb_endpoint_descriptor_t *)buf;
    ep_desc->bEndpointAddress_number += out_ep_num_base;
    if (lusbd_add_descriptor(class_idx, (uint8_t *)ep_desc, sizeof(cdcacm_data_out_ep_desc)))
        return -1;

    memcpy(buf, &cdcacm_data_in_ep_desc, sizeof(cdcacm_data_in_ep_desc));
    ep_desc = (usb_endpoint_descriptor_t *)buf;
    ep_desc->bEndpointAddress_number += in_ep_num_base;
    if (lusbd_add_descriptor(class_idx, (uint8_t *)ep_desc, sizeof(cdcacm_data_in_ep_desc)))
        return -1;

#if LUSBD_MAX_SPEED == LUSBD_HIGH_SPEED
    class_idx = lusbd_register_class(dev_idx, cfg_val, 1, &cdcacmd_class_cb, (void *)cdcacmd_count);
    if (0 > class_idx)
        return -1;
    if (lusbd_alloc_interface(class_idx, 2, &if_num_base))
        return -1;
    if (lusbd_add_endpoint(class_idx, 0, 0, LUSBD_DIR_IN, 1, LUSBD_EP_TYPE_INTERRUPT, 8, &hs_in_ep_num_base))
        return -1;
    if (lusbd_add_endpoint(class_idx, 1, 0, LUSBD_DIR_OUT, 1, LUSBD_EP_TYPE_BULK, USB_EP_MAX_PACKET_SIZE_HS_BULK, &hs_out_ep_num_base))
        return -1;
    if (lusbd_add_endpoint(class_idx, 1, 0, LUSBD_DIR_IN, 2, LUSBD_EP_TYPE_BULK, USB_EP_MAX_PACKET_SIZE_HS_BULK, NULL))
        return -1;

    memcpy(buf, &cdcacm_iad_desc, sizeof(cdcacm_iad_desc));
    iad_desc = (usb_interface_association_descriptor_t *)buf;
    iad_desc->bFirstInterface += if_num_base;
    iad_desc->iFunction = i_func;
    if (lusbd_add_descriptor(class_idx, (uint8_t *)iad_desc, sizeof(cdcacm_iad_desc)))
        return -1;

    memcpy(buf, &cdcacm_comm_if_desc, sizeof(cdcacm_comm_if_desc));
    if_desc = (usb_interface_descriptor_t *)buf;
    if_desc->bInterfaceNumber += if_num_base;
    if_desc->iInterface = i_if_comm;
    if (lusbd_add_descriptor(class_idx, (uint8_t *)if_desc, sizeof(cdcacm_comm_if_desc)))
        return -1;

    if (lusbd_add_descriptor(class_idx, (uint8_t *)&cdcacm_header_desc, sizeof(cdcacm_header_desc)))
        return -1;
    if (lusbd_add_descriptor(class_idx, (uint8_t *)&cdcacm_acm_desc, sizeof(cdcacm_acm_desc)))
        return -1;

    memcpy(buf, &cdcacm_union_desc, sizeof(cdcacm_union_desc));
    union_desc = (cdcacm_union_desc_t *)buf;
    union_desc->bControlInterface += if_num_base;
    union_desc->bSubordinateInterface0 += if_num_base;
    if (lusbd_add_descriptor(class_idx, (uint8_t *)union_desc, sizeof(cdcacm_union_desc)))
        return -1;

    memcpy(buf, &cdcacm_cm_desc, sizeof(cdcacm_cm_desc));
    cm_desc = (cdcacm_cm_desc_t *)buf;
    cm_desc->bDataInterface += if_num_base;
    if (lusbd_add_descriptor(class_idx, (uint8_t *)cm_desc, sizeof(cdcacm_cm_desc)))
        return -1;

    memcpy(buf, &cdcacm_cmd_ep_desc, sizeof(cdcacm_cmd_ep_desc));
    ep_desc = (usb_endpoint_descriptor_t *)buf;
    ep_desc->bEndpointAddress_number += hs_in_ep_num_base;
    if (lusbd_add_descriptor(class_idx, (uint8_t *)ep_desc, sizeof(cdcacm_cmd_ep_desc)))
        return -1;

    memcpy(buf, &cdcacm_data_if_desc, sizeof(cdcacm_data_if_desc));
    if_desc = (usb_interface_descriptor_t *)buf;
    if_desc->bInterfaceNumber += if_num_base;
    if_desc->iInterface = i_if_data;
    if (lusbd_add_descriptor(class_idx, (uint8_t *)if_desc, sizeof(cdcacm_data_if_desc)))
        return -1;

    memcpy(buf, &cdcacm_data_out_ep_desc, sizeof(cdcacm_data_out_ep_desc));
    ep_desc = (usb_endpoint_descriptor_t *)buf;
    ep_desc->bEndpointAddress_number += hs_out_ep_num_base;
    ep_desc->wMaxPacketSize_l = USB_EP_MAX_PACKET_SIZE_HS_BULK & 0xff;
    ep_desc->wMaxPacketSize_h = USB_EP_MAX_PACKET_SIZE_HS_BULK >> 8;
    if (lusbd_add_descriptor(class_idx, (uint8_t *)ep_desc, sizeof(cdcacm_data_out_ep_desc)))
        return -1;

    memcpy(buf, &cdcacm_data_in_ep_desc, sizeof(cdcacm_data_in_ep_desc));
    ep_desc = (usb_endpoint_descriptor_t *)buf;
    ep_desc->bEndpointAddress_number += hs_in_ep_num_base;
    ep_desc->wMaxPacketSize_l = USB_EP_MAX_PACKET_SIZE_HS_BULK & 0xff;
    ep_desc->wMaxPacketSize_h = USB_EP_MAX_PACKET_SIZE_HS_BULK >> 8;
    if (lusbd_add_descriptor(class_idx, (uint8_t *)ep_desc, sizeof(cdcacm_data_in_ep_desc)))
        return -1;
#endif

    cdcacmd->dev_idx = dev_idx;
    cdcacmd->cfg_val = cfg_val;
    cdcacmd->out_ep_num_base = out_ep_num_base;
    cdcacmd->in_ep_num_base = in_ep_num_base;
#if LUSBD_MAX_SPEED == LUSBD_HIGH_SPEED
    cdcacmd->hs_out_ep_num_base = hs_out_ep_num_base;
    cdcacmd->hs_in_ep_num_base = hs_in_ep_num_base;
#endif
    cdcacmd->cb = cb;

    if (   cdcacmd->cb
        && cdcacmd->cb->get_serial_config
        && (0 == cdcacmd->cb->get_serial_config(cdcacmd_count, &serial_config)))
    {
        cdcacmd->line_coding.dwDTERate_0 = serial_config.baud_rate & 0xff;
        cdcacmd->line_coding.dwDTERate_1 = (serial_config.baud_rate >> 8) & 0xff;
        cdcacmd->line_coding.dwDTERate_2 = (serial_config.baud_rate >> 16) & 0xff;
        cdcacmd->line_coding.dwDTERate_3 = (serial_config.baud_rate >> 24) & 0xff;
        cdcacmd->line_coding.bCharFormat = serial_config.stop_bits;
        cdcacmd->line_coding.bParityType = serial_config.parity_type;
        cdcacmd->line_coding.bDataBits = serial_config.data_bits;
    }
    else
    {
        cdcacmd->line_coding.dwDTERate_0 = 115200 & 0xff;
        cdcacmd->line_coding.dwDTERate_1 = (115200 >> 8) & 0xff;
        cdcacmd->line_coding.dwDTERate_2 = (115200 >> 16) & 0xff;
        cdcacmd->line_coding.dwDTERate_3 = (115200 >> 24) & 0xff;
        cdcacmd->line_coding.bCharFormat = CDCACM_LINE_CODING_FORMAT_STOP_BIT_1;
        cdcacmd->line_coding.bParityType = CDCACM_LINE_CODING_PARITY_TYPE_NONE;
        cdcacmd->line_coding.bDataBits = 8;
    }

    cdcacmd_count++;
    return cdcacmd_count - 1;
}

int cdcacmd_recv(unsigned int cdc_idx, uint8_t *buf, unsigned int len)
{
    cdcacmd_t *cdcacmd;

    if ((cdcacmd_count <= cdc_idx) || (NULL == buf) || (((uintptr_t)buf) & 0x03) || (0 == len))
        return -1;

    cdcacmd = &cdcacmd_list[cdc_idx];
    if (0 == cdcacmd->active)
        return -1;
    return lusbd_ep_rx(cdcacmd->dev_idx, cdcacmd->cur_out_ep_num_base + CDCACM_DATA_OUT_EP_NUM, buf, len);
}

int cdcacmd_send(unsigned int cdc_idx, const void *buf, unsigned int len)
{
    cdcacmd_t *cdcacmd;

    if ((cdcacmd_count <= cdc_idx) || (NULL == buf) || (((uintptr_t)buf) & 0x03) || (0 == len))
        return -1;

    cdcacmd = &cdcacmd_list[cdc_idx];
    if (0 == cdcacmd->active)
        return -1;
    return lusbd_ep_tx(cdcacmd->dev_idx, cdcacmd->cur_in_ep_num_base + CDCACM_DATA_IN_EP_NUM, buf, len, 1);
}
