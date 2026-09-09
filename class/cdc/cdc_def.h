#ifndef _CDC_DEF_H_
#define _CDC_DEF_H_

#include <stdint.h>


#define USB_DEVICE_CLASS_COMM  0x02  /* CDC */
#define USB_COMM_DEV_SUBCLASS_NONE  0x00
#define USB_COMM_DEV_PROTOCOL_NONE  0x00

#define USB_INTERFACE_CLASS_COMM  0x02
#define USB_COMM_IF_SUBCLASS_DLCM  0x01  /* Direct Line Control Model */
#define USB_COMM_IF_SUBCLASS_ACM   0x02  /* Abstract Control Model */
#define USB_COMM_IF_SUBCLASS_TCM   0x03  /* Telephone Control Model */
#define USB_COMM_IF_SUBCLASS_MCCM  0x04  /* Multi-Channel Control Mode */
#define USB_COMM_IF_SUBCLASS_CCM   0x05  /* CAPI Control Model */
#define USB_COMM_IF_SUBCLASS_ECM   0x06  /* Ethernet Networking Control Model */
#define USB_COMM_IF_SUBCLASS_ANCM  0x07  /* ATM Networking Control Model */
#define USB_COMM_IF_SUBCLASS_WHCM  0x08  /* Wireless Handset Control Model */
#define USB_COMM_IF_SUBCLASS_DM    0x09  /* Device Management */
#define USB_COMM_IF_SUBCLASS_MDLM  0x0A  /* Mobile Direct Line Model */
#define USB_COMM_IF_SUBCLASS_OBEX  0x0B
#define USB_COMM_IF_SUBCLASS_EEM   0x0C  /* Ethernet Emulation Model */
#define USB_COMM_IF_SUBCLASS_NCM   0x0D  /* Network Control Model */
#define USB_COMM_IF_PROTOCOL_NONE         0x00
#define USB_COMM_IF_PROTOCOL_AT_V250      0x01
#define USB_COMM_IF_PROTOCOL_AT_PCCA101   0x02
#define USB_COMM_IF_PROTOCOL_AT_PCCA101O  0x03
#define USB_COMM_IF_PROTOCOL_AT_GSM0707   0x04
#define USB_COMM_IF_PROTOCOL_AT_3GPP2707  0x05
#define USB_COMM_IF_PROTOCOL_AT_TIACDMA   0x06
#define USB_COMM_IF_PROTOCOL_AT_EEM       0x07  /* Ethernet Emulation Model */
#define USB_COMM_IF_PROTOCOL_AT_EXTERNAL  0xFE
#define USB_COMM_IF_PROTOCOL_VENDOR       0xFF

#define USB_INTERFACE_CLASS_DATA  0x0A
#define USB_DATA_IF_SUBCLASS_NONE  0x00
#define USB_DATA_IF_PROTOCOL_NONE         0x00
#define USB_DATA_IF_PROTOCOL_NTB          0x01  /* Network Transfer Block */
#define USB_DATA_IF_PROTOCOL_ISDNBRI      0x30
#define USB_DATA_IF_PROTOCOL_HDLC         0x31
#define USB_DATA_IF_PROTOCOL_TRANSPARENT  0x32
#define USB_DATA_IF_PROTOCOL_Q921M        0x50
#define USB_DATA_IF_PROTOCOL_Q921         0x51
#define USB_DATA_IF_PROTOCOL_Q921TM       0x52
#define USB_DATA_IF_PROTOCOL_V42BIS       0x90
#define USB_DATA_IF_PROTOCOL_EUROISDN     0x91  /* Q.931 */
#define USB_DATA_IF_PROTOCOL_V120         0x92
#define USB_DATA_IF_PROTOCOL_CAPI20       0x93
#define USB_DATA_IF_PROTOCOL_HOST_DRIVER  0xFD
#define USB_DATA_IF_PROTOCOL_PUFD         0xFE
#define USB_DATA_IF_PROTOCOL_VENDOR       0xFF

#define COMM_IF_FUNC_DESC_ID_HEADER           0x00
#define COMM_IF_FUNC_DESC_ID_CM               0x01  /* Call Management */
#define COMM_IF_FUNC_DESC_ID_ACM              0x02  /* Abstract Control Management */
#define COMM_IF_FUNC_DESC_ID_DLM              0x03  /* Direct Line Management */
#define COMM_IF_FUNC_DESC_ID_TEL_RINGER       0x04  /* Telephone Ringer */
#define COMM_IF_FUNC_DESC_ID_TEL_STATE        0x05  /* Telephone Call and Line State Reporting Capabilities */
#define COMM_IF_FUNC_DESC_ID_UNION            0x06
#define COMM_IF_FUNC_DESC_ID_COUNTRY_SEL      0x07  /* Country Selection */
#define COMM_IF_FUNC_DESC_ID_TEL_MODE         0x08  /* Telephone Operational Modes */
#define COMM_IF_FUNC_DESC_ID_USB_TERMINAL     0x09
#define COMM_IF_FUNC_DESC_ID_NET_TERMINAL     0x0A  /* Network Channel Terminal */
#define COMM_IF_FUNC_DESC_ID_PROTOCOL_UNIT    0x0B
#define COMM_IF_FUNC_DESC_ID_EXTENSION_UNIT   0x0C
#define COMM_IF_FUNC_DESC_ID_MCM              0x0D  /* Multi-Channel Management */
#define COMM_IF_FUNC_DESC_ID_CCM              0x0E  /* CAPI Control Management */
#define COMM_IF_FUNC_DESC_ID_ETH_NET          0x0F  /* Ethernet Networking */
#define COMM_IF_FUNC_DESC_ID_ATM_NET          0x10  /* ATM Networking */
#define COMM_IF_FUNC_DESC_ID_WHCM             0x11  /* Wireless Handset Control Model */
#define COMM_IF_FUNC_DESC_ID_MDLM             0x12  /* Mobile Direct Line Model */
#define COMM_IF_FUNC_DESC_ID_MDLM_DETAIL      0x13
#define COMM_IF_FUNC_DESC_ID_DMM              0x14  /* Device Management Model */
#define COMM_IF_FUNC_DESC_ID_OBEX             0x15
#define COMM_IF_FUNC_DESC_ID_CMD_SET          0x16  /* Command Set */
#define COMM_IF_FUNC_DESC_ID_CMD_SET_DETAIL   0x17  /* Command Set Detail */
#define COMM_IF_FUNC_DESC_ID_TCM              0x18  /* Telephone Control Model */
#define COMM_IF_FUNC_DESC_ID_OBEX_SERVICE_ID  0x19  /* OBEX Service Identifier */
#define COMM_IF_FUNC_DESC_ID_NCM              0x1A

#define COMM_IF_REQ_SEND_ENCAPSULATED_COMMAND  0x00
#define COMM_IF_REQ_GET_ENCAPSULATED_RESPONSE  0x01
#define COMM_IF_REQ_SET_COMM_FEATURE           0x02
#define COMM_IF_REQ_GET_COMM_FEATURE           0x03
#define COMM_IF_REQ_CLEAR_COMM_FEATURE         0x04
#define COMM_IF_REQ_SET_AUX_LINE_STATE         0x10
#define COMM_IF_REQ_SET_HOOK_STATE             0x11
#define COMM_IF_REQ_PULSE_SETUP                0x12
#define COMM_IF_REQ_SEND_PULSE                 0x13
#define COMM_IF_REQ_SET_PULSE_TIME             0x14
#define COMM_IF_REQ_RING_AUX_JACK              0x15
#define COMM_IF_REQ_SET_LINE_CODING            0x20
#define COMM_IF_REQ_GET_LINE_CODING            0x21
#define COMM_IF_REQ_SET_CONTROL_LINE_STATE     0x22
#define COMM_IF_REQ_SEND_BREAK                 0x23
#define COMM_IF_REQ_SET_RINGER_PARMS           0x30
#define COMM_IF_REQ_GET_RINGER_PARMS           0x31
#define COMM_IF_REQ_SET_OPERATION_PARMS        0x32
#define COMM_IF_REQ_GET_OPERATION_PARMS        0x33
#define COMM_IF_REQ_SET_LINE_PARMS             0x34
#define COMM_IF_REQ_GET_LINE_PARMS             0x35
#define COMM_IF_REQ_DIAL_DIGITS                0x36
#define COMM_IF_REQ_SET_UNIT_PARAMETER         0x37
#define COMM_IF_REQ_GET_UNIT_PARAMETER         0x38
#define COMM_IF_REQ_CLEAR_UNIT_PARAMETER       0x39
#define COMM_IF_REQ_GET_PROFILE                0x3A
#define COMM_IF_REQ_SET_ETH_MULTICAST_FILTERS  0x40  /* SET_ETHERNET_MULTICAST_FILTERS */
#define COMM_IF_REQ_SET_ETH_POWER_FILTER       0x41  /* SET_ETHERNET_POWER_MANAGEMENT_PATTERN_FILTER */
#define COMM_IF_REQ_GET_ETH_POWER_FILTER       0x42  /* GET_ETHERNET_POWER_MANAGEMENT_PATTERN_FILTER */
#define COMM_IF_REQ_SET_ETH_PACKET_FILTER      0x43  /* SET_ETHERNET_PACKET_FILTER */
#define COMM_IF_REQ_GET_ETH_STATISTIC          0x44  /* GET_ETHERNET_STATISTIC */
#define COMM_IF_REQ_SET_ATM_DATA_FORMAT        0x50
#define COMM_IF_REQ_GET_ATM_DEVICE_STATISTICS  0x51
#define COMM_IF_REQ_SET_ATM_DEFAULT_VC         0x52
#define COMM_IF_REQ_GET_ATM_VC_STATISTICS      0x53
#define COMM_IF_REQ_GET_NTB_PARAMETERS         0x80
#define COMM_IF_REQ_GET_NET_ADDRESS            0x81
#define COMM_IF_REQ_SET_NET_ADDRESS            0x82
#define COMM_IF_REQ_GET_NTB_FORMAT             0x83
#define COMM_IF_REQ_SET_NTB_FORMAT             0x84
#define COMM_IF_REQ_GET_NTB_INPUT_SIZE         0x85
#define COMM_IF_REQ_SET_NTB_INPUT_SIZE         0x86
#define COMM_IF_REQ_GET_MAX_DATAGRAM_SIZE      0x87
#define COMM_IF_REQ_SET_MAX_DATAGRAM_SIZE      0x88
#define COMM_IF_REQ_GET_CRC_MODE               0x89
#define COMM_IF_REQ_SET_CRC_MODE               0x8A


typedef struct
{
    uint8_t bFunctionLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint8_t bcdCDC_l;
    uint8_t bcdCDC_h;
} comm_if_header_desc_t;

typedef struct
{
    uint8_t bFunctionLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint8_t bControlInterface;
    uint8_t bSubordinateInterface[];
} comm_if_union_desc_t;

typedef struct
{
    uint8_t bFunctionLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint8_t iCountryCodeRelDate;
    uint8_t wCountryCode[];
} comm_if_country_sel_desc_t;


#endif
