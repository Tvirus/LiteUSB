#include "liteusb_device_hal.h"
#include "liteusb_hal_cfg.h"
#include <string.h>
#include "misc.h"
#include "n32h49x_rcc.h"
#include "n32h49x_exti.h"
#include "n32h49x_gpio.h"
#include "usbfsd_lib.h"




#define PACKET_BUFFER_MEMORY_SIZE  512

typedef struct
{
    uint16_t rx_mps;
    uint16_t tx_mps;

    unsigned int rx_total_len;
    unsigned int rx_remaining_len;
    uint8_t *rx_buf;

    unsigned int tx_total_len;
    unsigned int tx_remaining_len;
    const uint8_t *tx_buf;
} hal_ep_t;
static hal_ep_t hal_ep_list[LUSBD_IN_EP_MAX_NUM + 1] = {0};
static uint8_t ep0_setup_buf[sizeof(usb_setup_data_t)] __attribute__((aligned(4)));
static signed int change_addr = -1;

#define EP0_TX_ADDR  64
#define EP0_RX_ADDR  128


static uint16_t get_n32_ep_type(unsigned int type)
{
    if (type == LUSBD_EP_TYPE_CONTROL)
        return EP_CONTROL;
    else if (type == LUSBD_EP_TYPE_ISOCHRONOUS)
        return EP_ISOCHRONOUS;
    else if (type == LUSBD_EP_TYPE_BULK)
        return EP_BULK;
    else if (type == LUSBD_EP_TYPE_INTERRUPT)
        return EP_INTERRUPT;
    else
        return EP_CONTROL;
}

static void reset_device(void)
{
    int i;

    memset(hal_ep_list, 0, sizeof(hal_ep_list));
    hal_ep_list[0].rx_mps = LUSBD_MAX_PACKET_SIZE_EP0;
    hal_ep_list[0].tx_mps = LUSBD_MAX_PACKET_SIZE_EP0;
    change_addr = -1;

    USB_SetBuftab(0);
    USB_SetEpType(ENDP0, EP_CONTROL);
    SetEPTxStatus(ENDP0, EP_TX_NAK);
    USB_SetEpTxAddr(ENDP0, EP0_TX_ADDR);
    USB_SetEpRxAddr(ENDP0, EP0_RX_ADDR);
    USB_ClrStsOut(ENDP0);
    USB_SetEpRxCnt(ENDP0, LUSBD_MAX_PACKET_SIZE_EP0);
    USB_SetEpRxValid(ENDP0);

    for (i = 0; i <= LUSBD_IN_EP_MAX_NUM; i++)
        USB_SetEpAddress(i, i);
    USB_SetAddr(ADDR_EFUC | 0);
}

int lusbd_hal_start_device(unsigned int dev_idx)
{
    GPIO_InitType GPIO_InitStructure;
    NVIC_InitType NVIC_InitStructure;
    EXTI_InitType EXTI_InitStructure;


    RCC_EnableAHB1PeriphClk(RCC_AHB_PERIPHEN_GPIOA, ENABLE);

    GPIO_InitStruct(&GPIO_InitStructure);

    GPIO_InitStructure.Pin = GPIO_PIN_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_MODE_AF_PP;
    GPIO_InitStructure.GPIO_Alternate = GPIO_AF10;
    GPIO_InitPeripheral(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.Pin  = GPIO_PIN_12;
    GPIO_InitStructure.GPIO_Mode = GPIO_MODE_AF_PP;
    GPIO_InitStructure.GPIO_Alternate = GPIO_AF10;
    GPIO_InitPeripheral(GPIOA, &GPIO_InitStructure);

    /* Enable the USB interrupt */
    NVIC_InitStructure.NVIC_IRQChannel = USB_FS_LP_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = IRQ_PREEMPTION_PRIORITY;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    /* Enable the USB Wake-up interrupt */
    NVIC_InitStructure.NVIC_IRQChannel = USB_FS_WKUP_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = IRQ_PREEMPTION_PRIORITY;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    /* Configure the EXTI line 18 connected internally to the USB IP */
    EXTI_ClrITPendBit(EXTI_LINE18);
    EXTI_InitStructure.EXTI_Line = EXTI_LINE18;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_InitPeripheral(&EXTI_InitStructure);

    RCC_ConfigUSBFSClk(RCC_USBFS_CLKSRC_PLLPRES);
    RCC_ConfigUSBPLLPresClk(RCC_USBPLLCLK_SRC_PLL, RCC_USBPLLCLK_DIV5);  /* For SYSCLK_VALUE_240MHz */
    RCC->CFG3 |= RCC_CFG3_USBFSTM;
    RCC_EnableAPB1PeriphClk(RCC_APB1_PERIPHEN_USBFS, ENABLE);

    /* power on */
    USB_SetCtrl(CTRL_FRST);
    USB_SetCtrl(0);
    USB_SetSts(0);
    //USB_SetCtrl(CTRL_CTRSM | CTRL_ERRORM | CTRL_WKUPM | CTRL_SUSPDM | CTRL_RSTM | CTRL_SOFM | CTRL_ESOFM);
    USB_SetCtrl(CTRL_CTRSM | CTRL_ERRORM | CTRL_WKUPM | CTRL_SUSPDM | CTRL_RSTM);

    reset_device();
    _EnPortPullup();
    return 0;
}
int lusbd_hal_stop_device(unsigned int dev_idx)
{
    USB_SetCtrl(CTRL_FRST);
    USB_SetSts(0);
    USB_SetCtrl(CTRL_FRST + CTRL_PD);
    return 0;
}

int lusbd_hal_ep_open(unsigned int dev_idx, unsigned int dir, unsigned int ep_num, unsigned int mps, unsigned int type)
{
    if (0 == ep_num)
        return 0;

    if (LUSBD_IN_EP_MAX_NUM < ep_num)
        return -1;

    USB_SetEpType(ep_num, get_n32_ep_type(type));
    if (LUSBD_DIR_IN == dir)
    {
        hal_ep_list[ep_num].tx_total_len = 0;
        hal_ep_list[ep_num].tx_remaining_len = 0;
        hal_ep_list[ep_num].tx_buf = NULL;
        hal_ep_list[ep_num].tx_mps = mps;
        SetEPTxStatus(ep_num, EP_TX_NAK);
    }
    else
    {
        hal_ep_list[ep_num].rx_total_len = 0;
        hal_ep_list[ep_num].rx_remaining_len = 0;
        hal_ep_list[ep_num].rx_buf = NULL;
        hal_ep_list[ep_num].rx_mps = mps;
        SetEPRxStatus(ep_num, EP_RX_NAK);
    }
    return 0;
}

int lusbd_hal_ep_set_halt(unsigned int dev_idx, unsigned int dir, unsigned int ep_num)
{
    if (LUSBD_IN_EP_MAX_NUM < ep_num)
        return -1;

    if (LUSBD_DIR_IN == dir)
    {
        hal_ep_list[ep_num].tx_total_len = 0;
        hal_ep_list[ep_num].tx_remaining_len = 0;
        hal_ep_list[ep_num].tx_buf = NULL;
        SetEPTxStatus(ep_num, EP_TX_STALL);
    }
    else
    {
        hal_ep_list[ep_num].rx_total_len = 0;
        hal_ep_list[ep_num].rx_remaining_len = 0;
        hal_ep_list[ep_num].rx_buf = NULL;
        SetEPRxStatus(ep_num, EP_RX_STALL);
    }
    return 0;
}

int lusbd_hal_ep_clear_halt(unsigned int dev_idx, unsigned int dir, unsigned int ep_num)
{
    if (LUSBD_IN_EP_MAX_NUM < ep_num)
        return -1;

    if (LUSBD_DIR_IN == dir)
    {
        USB_ClrDattogTx(ep_num);
        SetEPTxStatus(ep_num, EP_TX_NAK);
    }
    else
    {
        USB_ClrDattogRx(ep_num);
        SetEPRxStatus(ep_num, EP_RX_NAK);
    }
    return 0;
}

int lusbd_hal_ep_tx(unsigned int dev_idx, unsigned int ep_num, const void *buf, unsigned int len)
{
    unsigned int mps;

    if (0 == ep_num)
    {
        if (hal_ep_list[0].tx_mps < len)
            return -1;
        if (len)
            USB_CopyUserToPMABuf((uint8_t *)buf, USB_GetEpTxAddr(ENDP0), len);
        USB_SetEpTxCnt(ENDP0, len);
        USB_SetEpTxValid(ENDP0);
        return 0;
    }

    if (LUSBD_IN_EP_MAX_NUM < ep_num)
        return -1;

    hal_ep_list[ep_num].tx_buf = buf;
    hal_ep_list[ep_num].tx_remaining_len = len;
    hal_ep_list[ep_num].tx_total_len = len;
    mps = hal_ep_list[ep_num].tx_mps;
    if (mps <= len)
    {
        USB_CopyUserToPMABuf((uint8_t *)buf, USB_GetEpTxAddr(ep_num), mps);
        USB_SetEpTxCnt(ep_num, mps);
    }
    else
    {
        USB_CopyUserToPMABuf((uint8_t *)buf, USB_GetEpTxAddr(ep_num), len);
        USB_SetEpTxCnt(ep_num, len);
    }
    USB_SetEpTxValid(ep_num);
    return 0;
}

int lusbd_hal_ep_rx(unsigned int dev_idx, unsigned int ep_num, void *buf, unsigned int len)
{
    unsigned int mps;

    if (0 == ep_num)
    {
        if (hal_ep_list[0].rx_mps < len)
            return -1;
        hal_ep_list[0].rx_buf = buf;
        hal_ep_list[0].rx_total_len = len;
        USB_SetEpRxCnt(ENDP0, len);
        USB_SetEpRxValid(ENDP0);
        return 0;
    }

    if (LUSBD_OUT_EP_MAX_NUM < ep_num)
        return -1;

    hal_ep_list[ep_num].rx_buf = buf;
    hal_ep_list[ep_num].rx_remaining_len = len;
    hal_ep_list[ep_num].rx_total_len = len;
    mps = hal_ep_list[ep_num].rx_mps;
    if (mps <= len)
        USB_SetEpRxCnt(ep_num, mps);
    else
        USB_SetEpRxCnt(ep_num, len);
    USB_SetEpRxValid(ep_num);
    return 0;
}

int lusbd_hal_ep0_rx_setup(unsigned int dev_idx)
{
    USB_SetEpRxCnt(ENDP0, hal_ep_list[0].rx_mps);
    USB_SetEpRxValid(ENDP0);
    return 0;
}

int lusbd_hal_set_address(unsigned int dev_idx, unsigned int addr)
{
    change_addr = addr;
    return 0;
}

int lusbd_hal_init_eps(unsigned int dev_idx, const lusbd_ep_t *out_ep_list, unsigned int out_ep_count, const lusbd_ep_t *in_ep_list, unsigned int in_ep_count)
{
    const lusbd_ep_t *ep;
    unsigned int buf_offset = 128 + 64;
    unsigned int buf_size;
    int i;

    for (i = 1; i < out_ep_count; i++)
    {
        ep = &out_ep_list[i];
        if (0 == ep->used)
            continue;

        buf_size = (ep->max_alt_mps + 3) & ~0x03;
        if (PACKET_BUFFER_MEMORY_SIZE < buf_offset + buf_size)
        {
            LUSBD_ERROR("Failed to init eps, required PBM exceeds the max(%u)", PACKET_BUFFER_MEMORY_SIZE);
            return -1;
        }
        hal_ep_list[i].rx_mps = ep->max_packet_size;
        hal_ep_list[i].rx_total_len = 0;
        hal_ep_list[i].rx_remaining_len = 0;
        hal_ep_list[i].rx_buf = NULL;
        USB_SetEpRxAddr(i, buf_offset);
        USB_SetEpType(i, get_n32_ep_type(ep->type));
        SetEPRxStatus(i, EP_RX_NAK);

        LUSBD_DEBUG("Set out ep(%u) PBM addr:%u size:%u", i, buf_offset, buf_size);
        buf_offset += buf_size;
    }
    for (i = 1; i < in_ep_count; i++)
    {
        ep = &in_ep_list[i];
        if (0 == ep->used)
            continue;

        buf_size = (ep->max_alt_mps + 3) & ~0x03;
        if (PACKET_BUFFER_MEMORY_SIZE < buf_offset + buf_size)
        {
            LUSBD_ERROR("Failed to init eps, required PBM exceeds the max(%u)", PACKET_BUFFER_MEMORY_SIZE);
            return -1;
        }
        hal_ep_list[i].tx_mps = ep->max_packet_size;
        hal_ep_list[i].tx_total_len = 0;
        hal_ep_list[i].tx_remaining_len = 0;
        hal_ep_list[i].tx_buf = NULL;
        USB_SetEpTxAddr(i, buf_offset);
        USB_SetEpType(i, get_n32_ep_type(ep->type));
        SetEPTxStatus(i, EP_TX_NAK);

        LUSBD_DEBUG("Set in ep(%u) PBM addr:%u size:%u", i, buf_offset, buf_size);
        buf_offset += buf_size;
    }
    return 0;
}

int lusbd_hal_set_remote_wakeup(unsigned int dev_idx, unsigned int enable)
{
    return -1;
}

int lusbd_hal_set_test_mode(unsigned int dev_idx, unsigned int test_mode)
{
    return -1;
}




static void transfer_handler(void)
{
    uint16_t istr;
    unsigned int ep_num;
    uint16_t ep_reg;
    unsigned int mps;
    unsigned int total_len;
    unsigned int remaining_len;
    uint8_t *rx_buf;
    unsigned int recv_len;

    while ((istr = _GetISTR()) & STS_CTRS)
    {
        ep_num = (uint8_t)((istr & STS_EP_ID) >> 8);
        if (LUSBD_IN_EP_MAX_NUM < ep_num)
            return;
        ep_reg = _GetENDPOINT(ep_num);

        if (ep_num)
        {
            if (ep_reg & EP_CTRS_RX)
            {
                USB_ClrEpCtrsRx(ep_num);

                mps = hal_ep_list[ep_num].rx_mps;
                recv_len = USB_GetEpRxCnt(ep_num);
                total_len = hal_ep_list[ep_num].rx_total_len;
                remaining_len = hal_ep_list[ep_num].rx_remaining_len;
                rx_buf = hal_ep_list[ep_num].rx_buf;

                if ((0 == total_len) || (NULL == rx_buf))
                {
                    hal_ep_list[ep_num].rx_total_len = 0;
                    hal_ep_list[ep_num].rx_remaining_len = 0;
                    hal_ep_list[ep_num].rx_buf = NULL;
                    lusbd_data_out_handler(0, ep_num, rx_buf, total_len);
                }
                else if (remaining_len < recv_len)
                {
                    hal_ep_list[ep_num].rx_total_len = 0;
                    hal_ep_list[ep_num].rx_remaining_len = 0;
                    hal_ep_list[ep_num].rx_buf = NULL;
                    USB_CopyPMAToUserBuf(rx_buf + total_len - remaining_len, USB_GetEpRxAddr(ep_num), remaining_len);
                    lusbd_data_out_handler(0, ep_num, rx_buf, total_len);
                }
                else if ((mps > recv_len) || (remaining_len == recv_len))
                {
                    hal_ep_list[ep_num].rx_total_len = 0;
                    hal_ep_list[ep_num].rx_remaining_len = 0;
                    hal_ep_list[ep_num].rx_buf = NULL;
                    USB_CopyPMAToUserBuf(rx_buf + total_len - remaining_len, USB_GetEpRxAddr(ep_num), recv_len);
                    lusbd_data_out_handler(0, ep_num, rx_buf, total_len - remaining_len + recv_len);
                }
                else
                {
                    USB_CopyPMAToUserBuf(hal_ep_list[ep_num].rx_buf + total_len - remaining_len, USB_GetEpRxAddr(ep_num), mps);
                    hal_ep_list[ep_num].rx_remaining_len -= mps;
                    if (mps < hal_ep_list[ep_num].rx_remaining_len)
                        USB_SetEpRxCnt(ep_num, mps);
                    else
                        USB_SetEpRxCnt(ep_num, hal_ep_list[ep_num].rx_remaining_len);
                    USB_SetEpRxValid(ep_num);
                }
            }
            if (ep_reg & EP_CTRS_TX)
            {
                USB_ClrEpCtrsTx(ep_num);

                mps = hal_ep_list[ep_num].tx_mps;
                total_len = hal_ep_list[ep_num].tx_total_len;
                remaining_len = hal_ep_list[ep_num].tx_remaining_len;

                if (mps < remaining_len)
                {
                    hal_ep_list[ep_num].tx_remaining_len -= mps;
                    remaining_len = hal_ep_list[ep_num].tx_remaining_len;
                    if (mps < remaining_len)
                    {
                        USB_CopyUserToPMABuf((uint8_t *)hal_ep_list[ep_num].tx_buf + total_len - remaining_len, USB_GetEpTxAddr(ep_num), mps);
                        USB_SetEpTxCnt(ep_num, mps);
                    }
                    else
                    {
                        USB_CopyUserToPMABuf((uint8_t *)hal_ep_list[ep_num].tx_buf + total_len - remaining_len, USB_GetEpTxAddr(ep_num), remaining_len);
                        USB_SetEpTxCnt(ep_num, remaining_len);
                    }
                    USB_SetEpTxValid(ep_num);
                }
                else
                {
                    hal_ep_list[ep_num].tx_total_len = 0;
                    hal_ep_list[ep_num].tx_remaining_len = 0;
                    hal_ep_list[ep_num].tx_buf = NULL;
                    lusbd_data_in_handler(0, ep_num);
                }
            }
        }
        else
        {
            if (istr & STS_DIR)  /* OUT */
            {
                USB_ClrEpCtrsRx(ENDP0);

                recv_len = USB_GetEpRxCnt(ENDP0);
                if (ep_reg & EP_SETUP)
                {
                    hal_ep_list[0].rx_total_len = 0;
                    hal_ep_list[0].rx_buf = NULL;
                    hal_ep_list[0].tx_total_len = 0;
                    change_addr = -1;
                    if (sizeof(usb_setup_data_t) != recv_len)
                    {
                        SetEPRxStatus(ENDP0, EP_RX_STALL);
                        SetEPTxStatus(ENDP0, EP_TX_STALL);
                        LUSBD_ERROR("n32h49 setup packet len(%u) invalid, ep0 halted", USB_GetEpRxCnt(ENDP0));
                    }
                    else
                    {
                        SetEPTxStatus(ENDP0, EP_TX_NAK);
                        USB_CopyPMAToUserBuf(ep0_setup_buf, EP0_RX_ADDR, sizeof(usb_setup_data_t));
                        lusbd_setup_handler(0, ep0_setup_buf);
                    }
                }
                else if (ep_reg & EP_CTRS_RX)
                {
                    total_len = hal_ep_list[0].rx_total_len;
                    rx_buf = hal_ep_list[0].rx_buf;
                    hal_ep_list[0].rx_total_len = 0;
                    hal_ep_list[0].rx_buf = NULL;
                    if ((0 == total_len) || (NULL == rx_buf))
                    {
                        lusbd_data_out_handler(0, 0, NULL, 0);
                    }
                    else if (total_len >= recv_len)
                    {
                        USB_CopyPMAToUserBuf(rx_buf, USB_GetEpRxAddr(0), recv_len);
                        lusbd_data_out_handler(0, 0, rx_buf, recv_len);
                    }
                    else
                    {
                        USB_CopyPMAToUserBuf(rx_buf, USB_GetEpRxAddr(0), total_len);
                        lusbd_data_out_handler(0, 0, rx_buf, total_len);
                    }
                }
            }
            else  /* IN */
            {
                USB_ClrEpCtrsTx(ENDP0);
                if (0 <= change_addr)
                {
                    USB_SetAddr(ADDR_EFUC | change_addr);
                    change_addr = -1;
                }
                lusbd_data_in_handler(0, 0);
            }
        }
    }
}

void USB_FS_LP_IRQHandler(void)
{
    uint16_t istr;

    istr = _GetISTR();

    if (istr & STS_CTRS)
    {
        transfer_handler();
    }

    if (istr & STS_RST)
    {
        USB_SetSts((uint16_t)CLR_RST);
        reset_device();
        lusbd_reset_handler(0, LUSBD_FULL_SPEED);
    }

    if (istr & STS_DOVR)
    {
        USB_SetSts(CLR_DOVR);
    }

    if (istr & STS_ERROR)
    {
        USB_SetSts(CLR_ERROR);
    }

    if (istr & STS_WKUP)
    {
        USB_SetSts(CLR_WKUP);
        lusbd_resume_handler(0);
    }

    if (istr & STS_SUSPD)
    {
        USB_SetSts(CLR_SUSPD);
        lusbd_suspend_handler(0);
    }

    if (istr & STS_SOF)
    {
        USB_SetSts(CLR_SOF);
    }

    if (istr & STS_ESOF)
    {
        USB_SetSts(CLR_ESOF);
    }

}

void USB_FS_WKUP_IRQHandler(void)
{
    EXTI_ClrITPendBit(EXTI_LINE18);
}
