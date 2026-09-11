#include "liteusb_device_hal.h"
#include "liteusb_hal_cfg.h"
#include <string.h>




/**
 * RxFIFO =  (5*控制端点数量 + 8) + ((所使用的最大USB数据包/4) + 用于状态信息的1) + (2*OUT端点数量) + 用于全局NAK的1
 */

extern PCD_HandleTypeDef hpcd_USB_OTG_FS;

static unsigned int ep0_rx_total_len = 0;
static unsigned int ep0_rx_remaining_len = 0;
static uint8_t *ep0_rx_addr = NULL;
static uint8_t ep0_rx_buf[LUSBD_MAX_PACKET_SIZE_EP0] __attribute__((aligned(4)));


int lusbd_hal_start_device(unsigned int dev_idx)
{
    if (HAL_PCDEx_SetRxFiFo(&hpcd_USB_OTG_FS, ((5 * 1) + 8) + ((LUSBD_MAX_PACKET_SIZE_EP0 / 4) + 1) + (2 * 1) + 1 + 8))
        return -1;
    if (HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 0, LUSBD_MAX_PACKET_SIZE_EP0 / 4))
        return -1;
    if (HAL_PCD_Start(&hpcd_USB_OTG_FS))
        return -1;
    return 0;
}
int lusbd_hal_stop_device(unsigned int dev_idx)
{
    if (HAL_PCD_Stop(&hpcd_USB_OTG_FS))
        return -1;
    return 0;
}

int lusbd_hal_ep_open(unsigned int dev_idx, unsigned int dir, unsigned int num, unsigned int mps, unsigned int type)
{
    if (LUSBD_DIR_IN == dir)
        num |= 0x80;
    if (HAL_PCD_EP_Open(&hpcd_USB_OTG_FS, num, mps, type))
        return -1;
    return 0;
}

int lusbd_hal_ep_set_halt(unsigned int dev_idx, unsigned int dir, unsigned int num)
{
    if (LUSBD_DIR_IN == dir)
        num |= 0x80;
    if (HAL_PCD_EP_SetStall(&hpcd_USB_OTG_FS, (uint8_t)num))
        return -1;
    return 0;
}

int lusbd_hal_ep_clear_halt(unsigned int dev_idx, unsigned int dir, unsigned int num)
{
    if (LUSBD_DIR_IN == dir)
        num |= 0x80;
    if (HAL_PCD_EP_ClrStall(&hpcd_USB_OTG_FS, (uint8_t)num))
        return -1;
    return 0;
}

int lusbd_hal_ep_tx(unsigned int dev_idx, unsigned int num, const void *buf, unsigned int len)
{
    if (HAL_PCD_EP_Transmit(&hpcd_USB_OTG_FS, num, (uint8_t *)buf, len))
        return -1;
    return 0;
}

int lusbd_hal_ep_rx(unsigned int dev_idx, unsigned int num, void *buf, unsigned int len)
{
    unsigned int mps;

    if (num)
    {
        mps = hpcd_USB_OTG_FS.OUT_ep[num].maxpacket;
        if (0 == mps)
            return -1;
        if (len % mps)
        {
            LUSBD_ERROR("STM32U5 out ep(%u) rx buf size(%u) must be a multiple of MPS(%u)", num, len, mps);
            return -1;
        }
        if (HAL_PCD_EP_Receive(&hpcd_USB_OTG_FS, num, (uint8_t *)buf, len))
            return -1;
    }
    else
    {
        ep0_rx_total_len = len;
        ep0_rx_remaining_len = len;
        ep0_rx_addr = (uint8_t *)buf;
        if (0 == len)
        {
            if (HAL_PCD_EP_Receive(&hpcd_USB_OTG_FS, 0, ep0_rx_buf, 0))
                return -1;
        }
        else if (LUSBD_MAX_PACKET_SIZE_EP0 > len)
        {
            if (HAL_PCD_EP_Receive(&hpcd_USB_OTG_FS, 0, ep0_rx_buf, LUSBD_MAX_PACKET_SIZE_EP0))
                return -1;
        }
        else
        {
            if (HAL_PCD_EP_Receive(&hpcd_USB_OTG_FS, 0, ep0_rx_addr, LUSBD_MAX_PACKET_SIZE_EP0))
                return -1;
        }
    }
    return 0;
}

int lusbd_hal_set_address(unsigned int dev_idx, unsigned int addr)
{
    if (HAL_PCD_SetAddress(&hpcd_USB_OTG_FS, addr))
        return -1;
    return 0;
}

int lusbd_hal_init_eps(unsigned int dev_idx, const lusbd_ep_t *out_ep_list, unsigned int out_ep_count, const lusbd_ep_t *in_ep_list, unsigned int in_ep_count)
{
    const lusbd_ep_t *ep;
    unsigned int actual_out_count = 1;
    unsigned int max_out_mps = LUSBD_MAX_PACKET_SIZE_EP0;
    unsigned int isochronous = 0;
    unsigned int total_active_in_mps = 0;
    unsigned int total_in_mps = LUSBD_MAX_PACKET_SIZE_EP0;
    unsigned int rx_fifo;
    unsigned int rem_ram;
    unsigned int expanded_size;
    unsigned int max;
    int i;


    for (i = 1; i < out_ep_count; i++)
    {
        ep = &out_ep_list[i];
        if (0 == ep->used)
            continue;
        actual_out_count++;
        if (max_out_mps < ep->max_alt_mps)
            max_out_mps = ep->max_alt_mps;
        if (LUSBD_EP_TYPE_ISOCHRONOUS == ep->type)
            isochronous++;
    }
    if (1 < isochronous)
        rx_fifo = ((5 * 1) + 8) + (((max_out_mps + 3) / 4) + 1) * 2 + (2 * actual_out_count) + 1;
    else
        rx_fifo = ((5 * 1) + 8) + (((max_out_mps + 3) / 4) + 1) + (2 * actual_out_count) + 1;
    rx_fifo += 8;  //按公式设置rxfifo时有问题

    for (i = 1; i < in_ep_count; i++)
    {
        if (0 == in_ep_list[i].used)
        {
            total_in_mps += 16 * 4;
            continue;
        }
        max = (in_ep_list[i].max_alt_mps + 3) & ~3;
        if (16 * 4 > max)
            max = 16 * 4;
        total_in_mps += max;
        total_active_in_mps += in_ep_list[i].max_alt_mps;
    }

    if ((1200 / 4) < (rx_fifo + total_in_mps / 4))
    {
        LUSBD_ERROR("Failed to init eps, total ram(%u) exceeds the max(%u)", rx_fifo * 4 + total_in_mps, 1200);
        return -1;
    }
    if (HAL_PCDEx_SetRxFiFo(&hpcd_USB_OTG_FS, rx_fifo))
    {
        LUSBD_ERROR("Set RxFiFo size %u failed", rx_fifo);
        return -1;
    }
    LUSBD_DEBUG("Set RxFiFo size %u", rx_fifo);

    rem_ram = 1200 - rx_fifo * 4 - total_in_mps;

    if (HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 0, LUSBD_MAX_PACKET_SIZE_EP0 / 4))
    {
        LUSBD_ERROR("Set ep0 TxFiFo size %u failed", LUSBD_MAX_PACKET_SIZE_EP0 / 4);
        return -1;
    }
    LUSBD_DEBUG("Set ep0 TxFiFo size %u", LUSBD_MAX_PACKET_SIZE_EP0 / 4);

    for (i = 1; i < in_ep_count; i++)
    {
        ep = &in_ep_list[i];
        if (0 == ep->used)
        {
            if (HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, i, 16))
            {
                LUSBD_ERROR("Set unused ep(%u) TxFiFo size 16 failed", i);
                return -1;
            }
            continue;
        }

        max = (ep->max_alt_mps + 3) & ~3;
        if (16 * 4 > max)
            max = 16 * 4;
        expanded_size = max + ((rem_ram * ep->max_alt_mps / total_active_in_mps) & ~3);
        if (HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, i, expanded_size / 4))
        {
            LUSBD_ERROR("Set ep(%u) TxFiFo size %u failed", i, expanded_size / 4);
            return -1;
        }
        LUSBD_DEBUG("Set ep(%u) TxFiFo size %u -> %u", i, ep->max_alt_mps, expanded_size / 4);
    }

    for (i = 1; i < out_ep_count; i++)
    {
        ep = &out_ep_list[i];
        if ((0 == ep->used) || (ep->alt_num))
            continue;
        if (HAL_PCD_EP_Open(&hpcd_USB_OTG_FS, i, ep->max_packet_size, ep->type))
        {
            LUSBD_ERROR("Open OUT ep(%u) failed", i);
            return -1;
        }
    }
    for (i = 1; i < in_ep_count; i++)
    {
        ep = &in_ep_list[i];
        if ((0 == ep->used) || (ep->alt_num))
            continue;
        if (HAL_PCD_EP_Open(&hpcd_USB_OTG_FS, i | 0x80, ep->max_packet_size, ep->type))
        {
            LUSBD_ERROR("Open IN ep(%u) failed", i);
            return -1;
        }
    }

    return 0;
}

int lusbd_hal_set_remote_wakeup(unsigned int dev_idx, unsigned int enable)
{
    if (enable)
        return HAL_PCD_ActivateRemoteWakeup(&hpcd_USB_OTG_FS);
    else
        return HAL_PCD_DeActivateRemoteWakeup(&hpcd_USB_OTG_FS);
}

int lusbd_hal_set_test_mode(unsigned int dev_idx, unsigned int test_mode)
{
    return HAL_PCD_SetTestMode(&hpcd_USB_OTG_FS, test_mode);
}




void HAL_PCD_ConnectCallback(PCD_HandleTypeDef *hpcd)
{
    lusbd_connect_handler(0);
}

void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef *hpcd)
{
    lusbd_disconnect_handler(0);
}

void HAL_PCD_SuspendCallback(PCD_HandleTypeDef *hpcd)
{
    lusbd_suspend_handler(0);
}

void HAL_PCD_ResumeCallback(PCD_HandleTypeDef *hpcd)
{
    lusbd_resume_handler(0);
}

void HAL_PCD_ResetCallback(PCD_HandleTypeDef *hpcd)
{
#if LUSBD_MAX_SPEED == LUSBD_HIGH_SPEED
    if (hpcd->Init.speed == PCD_SPEED_HIGH)
        lusbd_reset_handler(0, LUSBD_HIGH_SPEED);
    else if (hpcd->Init.speed == PCD_SPEED_FULL)
#endif
        lusbd_reset_handler(0, LUSBD_FULL_SPEED);
}

void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *hpcd)
{
    uint32_t USBx_BASE = (uint32_t)hpcd->Instance;

    /* Cancel any pending EP0 IN transfer */
    USBx_DEVICE->DIEPEMPMSK &= ~1U;
    HAL_PCD_EP_Abort(hpcd, 0x80);
    CLEAR_IN_EP_INTR(0, USB_OTG_DIEPINT_EPDISD | USB_OTG_DIEPINT_XFRC);
    HAL_PCD_EP_Flush(hpcd, 0x80);

    lusbd_setup_handler(0, (uint8_t *)hpcd->Setup);
}

void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
    lusbd_data_in_handler(0, epnum);
}

void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
    unsigned int recv_len;

    recv_len = HAL_PCD_EP_GetRxCount(hpcd, epnum);

    if (epnum)
    {
        lusbd_data_out_handler(0, epnum, hpcd->OUT_ep[epnum].xfer_buff - recv_len, recv_len);
        return;
    }

    if (LUSBD_MAX_PACKET_SIZE_EP0 < recv_len)
    {
        lusbd_ep_set_halt(0, 0, 0);
        LUSBD_ERROR("STM32U5 ep0 out packet len(%u) exceeds MPS(%u), ep0 halted", recv_len, LUSBD_MAX_PACKET_SIZE_EP0);
        return;
    }
    if (ep0_rx_remaining_len < recv_len)
    {
        lusbd_ep_set_halt(0, 0, 0);
        LUSBD_ERROR("STM32U5 ep0 recv len(%u) exceeds expected len(%u), ep0 halted",
                    ep0_rx_total_len - ep0_rx_remaining_len + recv_len, ep0_rx_total_len);
        return;
    }
    if (LUSBD_MAX_PACKET_SIZE_EP0 > ep0_rx_remaining_len)
    {
        if (ep0_rx_addr)
            memcpy(ep0_rx_addr + ep0_rx_total_len - ep0_rx_remaining_len, ep0_rx_buf, recv_len);
        lusbd_data_out_handler(0, 0, ep0_rx_addr, ep0_rx_total_len - ep0_rx_remaining_len + recv_len);
    }
    else
    {
        if ((LUSBD_MAX_PACKET_SIZE_EP0 > recv_len) || (LUSBD_MAX_PACKET_SIZE_EP0 == ep0_rx_remaining_len))
        {
            lusbd_data_out_handler(0, 0, ep0_rx_addr, ep0_rx_total_len - ep0_rx_remaining_len + recv_len);
        }
        else
        {
            ep0_rx_remaining_len -= LUSBD_MAX_PACKET_SIZE_EP0;
            if (LUSBD_MAX_PACKET_SIZE_EP0 > ep0_rx_remaining_len)
                HAL_PCD_EP_Receive(hpcd, 0, ep0_rx_buf, LUSBD_MAX_PACKET_SIZE_EP0);
            else
                HAL_PCD_EP_Receive(hpcd, 0, ep0_rx_addr + ep0_rx_total_len - ep0_rx_remaining_len, LUSBD_MAX_PACKET_SIZE_EP0);
        }
    }
}
