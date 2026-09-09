#ifndef _USB_DEF_H_
#define _USB_DEF_H_

#include <stdint.h>


#define USB_STATE_NOTATTACHED  0
#define USB_STATE_ATTACHED     1
#define USB_STATE_POWERED      2
#define USB_STATE_DEFAULT      3
#define USB_STATE_ADDRESS      4
#define USB_STATE_CONFIGURED   5
#define USB_STATE_SUSPENDED    6


#define USB_REQ_DIRECTION_OUT  0
#define USB_REQ_DIRECTION_IN   1
#define USB_REQ_TYPE_STANDARD  0
#define USB_REQ_TYPE_CLASS     1
#define USB_REQ_TYPE_VENDOR    2
#define USB_REQ_RECIPIENT_DEVICE     0
#define USB_REQ_RECIPIENT_INTERFACE  1
#define USB_REQ_RECIPIENT_ENDPOINT   2
#define USB_REQ_RECIPIENT_OTHER      3
#define USB_REQ_CODE_GET_STATUS         0
#define USB_REQ_CODE_CLEAR_FEATURE      1
#define USB_REQ_CODE_SET_FEATURE        3
#define USB_REQ_CODE_SET_ADDRESS        5
#define USB_REQ_CODE_GET_DESCRIPTOR     6
#define USB_REQ_CODE_SET_DESCRIPTOR     7
#define USB_REQ_CODE_GET_CONFIGURATION  8
#define USB_REQ_CODE_SET_CONFIGURATION  9
#define USB_REQ_CODE_GET_INTERFACE      10
#define USB_REQ_CODE_SET_INTERFACE      11
#define USB_REQ_CODE_SYNCH_FRAME        12
#define USB_REQ_FEATURE_ENDPOINT_HALT         0
#define USB_REQ_FEATURE_DEVICE_REMOTE_WAKEUP  1
#define USB_REQ_FEATURE_TEST_MODE             2
#define USB_TEST_MODE_J             1
#define USB_TEST_MODE_K             2
#define USB_TEST_MODE_SE0_NAK       3
#define USB_TEST_MODE_PACKET        4
#define USB_TEST_MODE_FORCE_ENABLE  5
typedef struct __attribute__((packed))
{
    union
    {
        uint8_t bmRequestType;
        struct
        {
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
            uint8_t bmRequestType_direction:1;
            uint8_t bmRequestType_type:2;
            uint8_t bmRequestType_recipient:5;
#else
            uint8_t bmRequestType_recipient:5;
            uint8_t bmRequestType_type:2;
            uint8_t bmRequestType_direction:1;
#endif
        };
    };
    uint8_t bRequest;
    uint8_t wValue_l;
    uint8_t wValue_h;
    uint8_t wIndex_l;
    uint8_t wIndex_h;
    uint8_t wLength_l;
    uint8_t wLength_h;
} usb_setup_data_t;


#define USB_DESC_TYPE_DEVICE                     1
#define USB_DESC_TYPE_CONFIGURATION              2
#define USB_DESC_TYPE_STRING                     3
#define USB_DESC_TYPE_INTERFACE                  4
#define USB_DESC_TYPE_ENDPOINT                   5
#define USB_DESC_TYPE_DEVICE_QUALIFIER           6
#define USB_DESC_TYPE_OTHER_SPEED_CONFIGURATION  7
#define USB_DESC_TYPE_INTERFACE_POWER            8
#define USB_DESC_TYPE_OTG                        9
#define USB_DESC_TYPE_DEBUG                      10
#define USB_DESC_TYPE_INTERFACE_ASSOCIATION      11
#define USB_DESC_TYPE_BOS                        15
#define USB_DESC_TYPE_CS_UNDEFINED               0x20
#define USB_DESC_TYPE_CS_DEVICE                  0x21
#define USB_DESC_TYPE_CS_CONFIGURATION           0x22
#define USB_DESC_TYPE_CS_STRING                  0x23
#define USB_DESC_TYPE_CS_INTERFACE               0x24
#define USB_DESC_TYPE_CS_ENDPOINT                0x25

typedef struct __attribute__((packed))
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bcdUSB_l;
    uint8_t bcdUSB_h;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize0;
    uint8_t idVendor_l;
    uint8_t idVendor_h;
    uint8_t idProduct_l;
    uint8_t idProduct_h;
    uint8_t bcdDevice_l;
    uint8_t bcdDevice_h;
    uint8_t iManufacturer;
    uint8_t iProduct;
    uint8_t iSerialNumber;
    uint8_t bNumConfigurations;
} usb_device_descriptor_t;

typedef struct __attribute__((packed))
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bcdUSB_l;
    uint8_t bcdUSB_h;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize0;
    uint8_t bNumConfigurations;
    uint8_t bReserved;
} usb_device_qualifier_descriptor_t;

typedef struct __attribute__((packed))
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t wTotalLength_l;
    uint8_t wTotalLength_h;
    uint8_t bNumInterfaces;
    uint8_t bConfigurationValue;
    uint8_t iConfiguration;
    union
    {
        uint8_t bmAttributes;
        struct
        {
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
            uint8_t bmAttributes_rsv1:1;
            uint8_t bmAttributes_self_powered:1;
            uint8_t bmAttributes_remote_wakeup:1;
            uint8_t bmAttributes_rsv0:5;
#else
            uint8_t bmAttributes_rsv0:5;
            uint8_t bmAttributes_remote_wakeup:1;
            uint8_t bmAttributes_self_powered:1;
            uint8_t bmAttributes_rsv1:1;
#endif
        };
    };
    uint8_t bMaxPower;
} usb_configuration_descriptor_t, usb_other_speed_configuration_descriptor_t;

typedef struct __attribute__((packed))
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bFirstInterface;
    uint8_t bInterfaceCount;
    uint8_t bFunctionClass;
    uint8_t bFunctionSubClass;
    uint8_t bFunctionProtocol;
    uint8_t iFunction;
} usb_interface_association_descriptor_t;

typedef struct __attribute__((packed))
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
} usb_interface_descriptor_t;

#define USB_EP_DESC_DIRECTION_OUT  0
#define USB_EP_DESC_DIRECTION_IN   1
#define USB_EP_DESC_USAGE_TYPE_DATA      0
#define USB_EP_DESC_USAGE_TYPE_FEEDBACK  1
#define USB_EP_DESC_USAGE_TYPE_IMPLICIT  2
#define USB_EP_DESC_USAGE_TYPE_RSV       3
#define USB_EP_DESC_SYNC_TYPE_NOSYNC    0
#define USB_EP_DESC_SYNC_TYPE_ASYNC     1
#define USB_EP_DESC_SYNC_TYPE_ADAPTIVE  2
#define USB_EP_DESC_SYNC_TYPE_SYNC      3
#define USB_EP_DESC_TRANSFER_TYPE_CONTROL      0
#define USB_EP_DESC_TRANSFER_TYPE_ISOCHRONOUS  1
#define USB_EP_DESC_TRANSFER_TYPE_BULK         2
#define USB_EP_DESC_TRANSFER_TYPE_INTERRUPT    3
#define USB_EP_MAX_PACKET_SIZE_HS_BULK  512
typedef struct __attribute__((packed))
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    union
    {
        uint8_t bEndpointAddress;
        struct
        {
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
            uint8_t bEndpointAddress_direction:1;
            uint8_t bEndpointAddress_rsv:3;
            uint8_t bEndpointAddress_number:4;
#else
            uint8_t bEndpointAddress_number:4;
            uint8_t bEndpointAddress_rsv:3;
            uint8_t bEndpointAddress_direction:1;
#endif
        };
    };
    union
    {
        uint8_t bmAttributes;
        struct
        {
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
            uint8_t bmAttributes_rsv:2;
            uint8_t bmAttributes_usage_type:2;
            uint8_t bmAttributes_synchronization_type:2;
            uint8_t bmAttributes_transfer_type:2;
#else
            uint8_t bmAttributes_transfer_type:2;
            uint8_t bmAttributes_synchronization_type:2;
            uint8_t bmAttributes_usage_type:2;
            uint8_t bmAttributes_rsv:2;
#endif
        };
    };
    uint8_t wMaxPacketSize_l;
    uint8_t wMaxPacketSize_h;
    uint8_t bInterval;
} usb_endpoint_descriptor_t;


#define USB_DEV_CLASS_PER_INTERFACE   0x00
#define USB_DEV_CLASS_COMMUNICATIONS  0x02
#define USB_DEV_CLASS_HUB             0x09
#define USB_DEV_CLASS_BILLBOARD       0x11
#define USB_DEV_CLASS_DIAGNOSTIC      0xDC
#define USB_DEV_CLASS_MISCELLANEOUS   0xEF
#define USB_DEV_CLASS_VENDOR  0xFF

#define IAD_DEV_CLASS     USB_DEV_CLASS_MISCELLANEOUS
#define IAD_DEV_SUBCLASS  0x02
#define IAD_DEV_PROTOCOL  0x01


typedef struct __attribute__((packed))
{
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    uint8_t :6;
    uint8_t RemoteWakeup:1;
    uint8_t SelfPowered:1;
#else
    uint8_t SelfPowered:1;
    uint8_t RemoteWakeup:1;
    uint8_t :6;
#endif
    uint8_t rsv;
} usb_device_status_t;

typedef struct __attribute__((packed))
{
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    uint8_t :7;
    uint8_t Halt:1;
#else
    uint8_t Halt:1;
    uint8_t :7;
#endif
    uint8_t rsv;
} usb_endpoint_status_t;


#endif
