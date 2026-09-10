#include "liteusb_device.h"
#include "liteusb_device_hal.h"
#include <string.h>




typedef struct
{
    uint8_t len;
    const uint8_t *buf;
} lusbd_str_t;

typedef struct
{
    uint8_t used;
    uint8_t class_idx;

    uint8_t if_num;
    uint8_t cur_alt;
} lusbd_if_t;

typedef struct
{
    uint8_t cfg_idx;

    uint8_t if_num_base;
    uint8_t out_ep_num_base;
    uint8_t in_ep_num_base;
    const lusbd_class_cb_t *cb;
    void *class_data;
} lusbd_class_t;

typedef struct
{
    uint8_t dev_idx;
    uint8_t cfg_val;
    uint8_t is_hs;

    uint8_t max_out_ep_num;
    uint8_t max_in_ep_num;
    union __attribute__((aligned(4)))
    {
        usb_configuration_descriptor_t cfg_desc;
        uint8_t cfg_desc_buf[LUSBD_CFG_DESC_LEN];
    };
} lusbd_cfg_t;

typedef struct
{
    usb_device_descriptor_t dev_desc __attribute__((aligned(4)));
#if LUSBD_MAX_SPEED == LUSBD_HIGH_SPEED
    usb_device_qualifier_descriptor_t dev_qualifier_desc __attribute__((aligned(4)));
#endif

    uint8_t dev_state;
    uint8_t suspend;
    uint8_t enum_speed;
    uint8_t cur_cfg_val;
    uint8_t cur_cfg_idx;
    lusbd_user_cb_t user_cb;

    const uint8_t *ep0_tx_buf;
    unsigned int ep0_tx_len;
    lusbd_control_out_cb_t control_out_cb;
    void *control_out_arg;
    void *control_out_class_data;
    uint8_t need_zlp;
    uint8_t is_rx_status;
    uint8_t self_powered;
    uint8_t remote_wakeup;
    uint8_t test_mode;
    uint8_t test_mode_state;

    lusbd_if_t active_ifs[LUSBD_IF_LIST_LEN];
#if LUSBD_IN_EP_MAX_NUM <= LUSBD_OUT_EP_MAX_NUM
    lusbd_ep_t active_eps[2][LUSBD_OUT_EP_MAX_NUM + 1];
#else
    lusbd_ep_t active_eps[2][LUSBD_IN_EP_MAX_NUM + 1];
#endif

    union __attribute__((aligned(4)))
    {
        usb_device_status_t device_status;
        uint8_t interface_status[2];
        usb_endpoint_status_t endpoint_status;
        uint8_t cfg_val;
        uint8_t alt_setting;
    } tx_buf;
} lusbd_dev_t;


static lusbd_str_t lusbd_str_list[LUSBD_STR_MAX_COUNT] = {0};
static unsigned int str_count = 0;
static lusbd_dev_t lusbd_dev_list[LUSBD_DEV_COUNT] = {0};
static lusbd_cfg_t lusbd_cfg_list[LUSBD_CFG_LIST_LEN] = {0};
static unsigned int cfg_count = 0;
static lusbd_class_t lusbd_class_list[LUSBD_CLASS_LIST_LEN] = {0};
static unsigned int class_count = 0;
static lusbd_if_t lusbd_if_list[LUSBD_IF_LIST_LEN] = {0};
static unsigned int if_count = 0;
static lusbd_ep_t lusbd_ep_list[LUSBD_EP_LIST_LEN] = {0};
static unsigned int ep_count = 0;
const static uint8_t lang_id_desc[] __attribute__((aligned(4))) =
{
    4,
    USB_DESC_TYPE_STRING,
    0x09,
    0x04
};


int lusbd_add_str(const uint8_t *buf, unsigned int len)
{
    if ((NULL == buf) || (((uintptr_t)buf) & 0x03) || (253 < len))
        return -1;
    if (LUSBD_STR_MAX_COUNT <= str_count)
    {
        LUSBD_ERROR("Failed to add string, increase LUSBD_STR_MAX_COUNT(%u)", LUSBD_STR_MAX_COUNT);
        return -1;
    }

    lusbd_str_list[str_count].len = len;
    lusbd_str_list[str_count].buf = buf;
    str_count++;
    return (int)str_count;
}

int lusbd_set_dev_info(unsigned int dev_idx, uint16_t vid, uint16_t pid, uint16_t ver, unsigned int i_mf, unsigned int i_prod, unsigned int i_sn)
{
    lusbd_dev_t *dev;

    if (   (LUSBD_DEV_COUNT <= dev_idx)
        || (LUSBD_STR_MAX_COUNT < i_mf)
        || (LUSBD_STR_MAX_COUNT < i_prod)
        || (LUSBD_STR_MAX_COUNT < i_sn))
        return -1;
    dev = &lusbd_dev_list[dev_idx];

    dev->dev_desc.idVendor_l = vid & 0xff;
    dev->dev_desc.idVendor_h = vid >> 8;
    dev->dev_desc.idProduct_l = pid & 0xff;
    dev->dev_desc.idProduct_h = pid >> 8;
    dev->dev_desc.bcdDevice_l = ver & 0xff;
    dev->dev_desc.bcdDevice_h = ver >> 8;
    dev->dev_desc.iManufacturer = i_mf;
    dev->dev_desc.iProduct = i_prod;
    dev->dev_desc.iSerialNumber = i_sn;

    return 0;
}

int lusbd_set_cfg_info(unsigned int dev_idx, unsigned int cfg_val, unsigned int is_hs, unsigned int i_cfg, unsigned int is_self_powered, unsigned int max_power)
{
    lusbd_cfg_t *cfg;
    int i;

    if ((LUSBD_DEV_COUNT <= dev_idx) || (0 == cfg_val) || (LUSBD_STR_MAX_COUNT < i_cfg))
        return -1;
#if LUSBD_MAX_SPEED != LUSBD_HIGH_SPEED
    if (is_hs)
    {
        LUSBD_INFO("Skipping setting High-Speed config info");
        return 0;
    }
#endif
    is_hs = !!is_hs;

    for (i = 0; i < LUSBD_CFG_LIST_LEN; i++)
    {
        cfg = &lusbd_cfg_list[i];
        if (cfg_count <= i)
        {
            cfg_count++;
            cfg->dev_idx = dev_idx;
            cfg->cfg_val = cfg_val;
            cfg->is_hs = is_hs;
            cfg->cfg_desc.bLength = sizeof(usb_configuration_descriptor_t);
            cfg->cfg_desc.bDescriptorType = USB_DESC_TYPE_CONFIGURATION;
            cfg->cfg_desc.bConfigurationValue = cfg_val;
            cfg->cfg_desc.bmAttributes_rsv1 = 1;
#ifdef LUSBD_SPT_REMOTE_WAKEUP
            cfg->cfg_desc.bmAttributes_remote_wakeup = 1;
#else
            cfg->cfg_desc.bmAttributes_remote_wakeup = 0;
#endif
            break;
        }
        if (   (cfg->dev_idx == dev_idx)
            && (cfg->cfg_val == cfg_val)
            && (cfg->is_hs == is_hs))
            break;
    }
    if (LUSBD_CFG_LIST_LEN <= i)
    {
        LUSBD_ERROR("Failed to set config descriptor info, increase LUSBD_CFG_LIST_LEN(%u)", LUSBD_CFG_LIST_LEN);
        return -1;
    }

    cfg->cfg_desc.iConfiguration = i_cfg;
    cfg->cfg_desc.bmAttributes_self_powered = !!is_self_powered;
    cfg->cfg_desc.bMaxPower = max_power / 2;
    return 0;
}

int lusbd_set_user_cb(unsigned int dev_idx, lusbd_user_cb_t cb)
{
    if (LUSBD_DEV_COUNT <= dev_idx)
        return -1;
    lusbd_dev_list[dev_idx].user_cb = cb;
    return 0;
}


int lusbd_register_class(unsigned int dev_idx, unsigned int cfg_val, unsigned int is_hs, const lusbd_class_cb_t *cb, void *class_data)
{
    lusbd_class_t *class;
    lusbd_cfg_t *cfg;
    int i;

    if ((0 == cfg_val) || (NULL == cb))
        return -1;
    if (LUSBD_DEV_COUNT <= dev_idx)
    {
        LUSBD_ERROR("Failed to register class, dev_idx(%u) exceeds device count(%u)", dev_idx, LUSBD_DEV_COUNT);
        return -1;
    }
#if LUSBD_MAX_SPEED != LUSBD_HIGH_SPEED
    if (is_hs)
    {
        LUSBD_INFO("Skipping High-Speed class registration");
        return -2;
    }
#endif
    is_hs = !!is_hs;

    if (LUSBD_CLASS_LIST_LEN <= class_count)
    {
        LUSBD_ERROR("Failed to register class, increase LUSBD_CLASS_LIST_LEN(%u)", LUSBD_CLASS_LIST_LEN);
        return -1;
    }
    class = &lusbd_class_list[class_count];

    for (i = 0; i < LUSBD_CFG_LIST_LEN; i++)
    {
        cfg = &lusbd_cfg_list[i];
        if (cfg_count <= i)
        {
            cfg_count++;
            cfg->dev_idx = dev_idx;
            cfg->cfg_val = cfg_val;
            cfg->is_hs = is_hs;
            cfg->cfg_desc.bLength = sizeof(usb_configuration_descriptor_t);
            cfg->cfg_desc.bDescriptorType = USB_DESC_TYPE_CONFIGURATION;
            cfg->cfg_desc.bConfigurationValue = cfg_val;
            cfg->cfg_desc.bmAttributes_rsv1 = 1;
#ifdef LUSBD_SPT_REMOTE_WAKEUP
            cfg->cfg_desc.bmAttributes_remote_wakeup = 1;
#else
            cfg->cfg_desc.bmAttributes_remote_wakeup = 0;
#endif
            break;
        }
        if (   (cfg->dev_idx == dev_idx)
            && (cfg->cfg_val == cfg_val)
            && (cfg->is_hs == is_hs))
            break;
    }
    if (LUSBD_CFG_LIST_LEN <= i)
    {
        LUSBD_ERROR("Failed to register class, increase LUSBD_CFG_LIST_LEN(%u)", LUSBD_CFG_LIST_LEN);
        return -1;
    }
    class->cfg_idx = i;
    class->if_num_base = cfg->cfg_desc.bNumInterfaces;
    class->out_ep_num_base = cfg->max_out_ep_num;
    class->in_ep_num_base = cfg->max_in_ep_num;
    class->class_data = class_data;
    class->cb = cb;
    class_count++;

    LUSBD_DEBUG("Register class, dev_idx:%u cfg_val:%u is_hs:%u class_idx:%u", dev_idx, cfg_val, is_hs, class_count - 1);
    return (int)(class_count - 1);
}

int lusbd_alloc_interface(unsigned int class_idx, unsigned int count, unsigned int *if_num_base)
{
    lusbd_cfg_t *cfg;
    lusbd_if_t *interface;
    int i;

    if (NULL == if_num_base)
        return -1;
    if (class_count <= class_idx)
    {
        LUSBD_ERROR("Failed to alloc interface, class_idx(%u) not registered", class_idx);
        return -1;
    }

    if ((LUSBD_IF_LIST_LEN - if_count) < count)
    {
        LUSBD_ERROR("Failed to alloc interface, increase LUSBD_IF_LIST_LEN(%u)", LUSBD_IF_LIST_LEN);
        return -1;
    }

    cfg = &lusbd_cfg_list[lusbd_class_list[class_idx].cfg_idx];
    if ((255 - cfg->cfg_desc.bNumInterfaces) < count)
    {
        LUSBD_ERROR("Failed to alloc interface, interface count exceeds the max(255)");
        return -1;
    }

    for (i = 0; i < count; i++)
    {
        interface = &lusbd_if_list[if_count + i];
        interface->used = 1;
        interface->class_idx = class_idx;
        interface->if_num = cfg->cfg_desc.bNumInterfaces + i;
        interface->cur_alt = 0;
    }
    cfg->cfg_desc.bNumInterfaces += count;
    if_count += count;
    *if_num_base = lusbd_class_list[class_idx].if_num_base;

    LUSBD_DEBUG("Alloc interface, class_idx:%u count:%u if_num_base:%u", class_idx, count, *if_num_base);
    return 0;
}

int lusbd_add_endpoint(unsigned int class_idx, unsigned int if_num, uint8_t alt_num, unsigned int dir, unsigned int ep_num, unsigned int type, unsigned int mps, unsigned int *ep_num_base)
{
    lusbd_class_t *ep_class;
    lusbd_cfg_t *cfg;
    lusbd_ep_t *ep;

    if ((0 == ep_num) || (8 > mps))
        return -1;
    dir = !!dir;

    if (class_count <= class_idx)
    {
        LUSBD_ERROR("Failed to add endpoint, class_idx(%u) not registered", class_idx);
        return -1;
    }
    ep_class = &lusbd_class_list[class_idx];
    cfg = &lusbd_cfg_list[ep_class->cfg_idx];
    if ((LUSBD_IF_LIST_LEN - ep_class->if_num_base) <= if_num)
    {
        LUSBD_ERROR("Failed to add endpoint, if_num(%u+%u) invalid", ep_class->if_num_base, if_num);
        return -1;
    }
    if (LUSBD_EP_LIST_LEN <= ep_count)
    {
        LUSBD_ERROR("Failed to add endpoint, increase LUSBD_EP_LIST_LEN(%u)", LUSBD_EP_LIST_LEN);
        return -1;
    }
    ep = &lusbd_ep_list[ep_count];

    if (LUSBD_DIR_OUT == dir)
    {
        if ((LUSBD_OUT_EP_MAX_NUM - ep_class->out_ep_num_base) < ep_num)
        {
            LUSBD_ERROR("Failed to add endpoint, out endpoint num(%u+%u) exceeds the max(%u)", ep_class->out_ep_num_base, ep_num, LUSBD_OUT_EP_MAX_NUM);
            return -1;
        }
        ep->ep_num = ep_class->out_ep_num_base + ep_num;
        if (cfg->max_out_ep_num < ep->ep_num)
            cfg->max_out_ep_num = ep->ep_num;
        if (ep_num_base)
            *ep_num_base = ep_class->out_ep_num_base;
    }
    else
    {
        if ((LUSBD_IN_EP_MAX_NUM - ep_class->in_ep_num_base) < ep_num)
        {
            LUSBD_ERROR("Failed to add endpoint, in endpoint num(%u+%u) exceeds the max(%u)", ep_class->in_ep_num_base, ep_num, LUSBD_IN_EP_MAX_NUM);
            return -1;
        }
        ep->ep_num = ep_class->in_ep_num_base + ep_num;
        if (cfg->max_in_ep_num < ep->ep_num)
            cfg->max_in_ep_num = ep->ep_num;
        if (ep_num_base)
            *ep_num_base = ep_class->in_ep_num_base;
    }

    ep->used = 1;
    ep->class_idx = class_idx;
    ep->if_num = ep_class->if_num_base + if_num;
    ep->alt_num = alt_num;
    ep->dir = dir;
    ep->type = type;
    ep->max_packet_size = mps;
    ep->max_alt_mps = mps;
    ep_count++;

    LUSBD_DEBUG("Add endpoint, class_idx:%u if_num:%u alt_num:%u dir:%u ep_num:%u type:%u mps:%u",
                class_idx, ep->if_num, alt_num, dir, ep->ep_num, type, mps);
    return 0;
}

int lusbd_add_descriptor(unsigned int class_idx, const uint8_t *buf, unsigned int len)
{
    lusbd_cfg_t *cfg;
    unsigned int total_len;

    if ((class_count <= class_idx) || (NULL == buf))
        return -1;

    cfg = &lusbd_cfg_list[lusbd_class_list[class_idx].cfg_idx];

    total_len = (cfg->cfg_desc.wTotalLength_h << 8) | cfg->cfg_desc.wTotalLength_l;
    if (sizeof(usb_configuration_descriptor_t) > total_len)
        total_len = sizeof(usb_configuration_descriptor_t);
    if ((LUSBD_CFG_DESC_LEN - total_len) < len)
    {
        LUSBD_ERROR("Failed to add descriptor, increase LUSBD_CFG_DESC_LEN");
        return -1;
    }

    memcpy(cfg->cfg_desc_buf + total_len, buf, len);
    total_len += len;
    cfg->cfg_desc.wTotalLength_l = total_len & 0xff;
    cfg->cfg_desc.wTotalLength_h = total_len >> 8;
    return 0;
}

void lusbd_print_res_usage(void)
{
    char *speed;
    unsigned int len;
    int i;

    LUSBD_PRINT("\n============== [ LiteUSB Device Resource ] =============\n");
    LUSBD_PRINT("String List   :  %2u / %2u\n", str_count, LUSBD_STR_MAX_COUNT);
    LUSBD_PRINT("Config List   :  %2u / %2u\n", cfg_count, LUSBD_CFG_LIST_LEN);
    LUSBD_PRINT("Class List    :  %2u / %2u\n", class_count, LUSBD_CLASS_LIST_LEN);
    LUSBD_PRINT("Interface List:  %2u / %2u\n", if_count, LUSBD_IF_LIST_LEN);
    LUSBD_PRINT("Endpoint List :  %2u / %2u\n\n", ep_count, LUSBD_EP_LIST_LEN);
    LUSBD_PRINT("DEV CFG HS      IN_EP(%u)  OUT_EP(%u)  DESC_BUF(%u)\n", LUSBD_IN_EP_MAX_NUM, LUSBD_OUT_EP_MAX_NUM, LUSBD_CFG_DESC_LEN);
    LUSBD_PRINT("-----------------------------------------------------\n");
    for (i = 0; i < LUSBD_CFG_LIST_LEN; i++)
    {
        if (0 == lusbd_cfg_list[i].cfg_val)
            break;
        len = (lusbd_cfg_list[i].cfg_desc.wTotalLength_h << 8) | lusbd_cfg_list[i].cfg_desc.wTotalLength_l;
        if (lusbd_cfg_list[i].is_hs)
            speed = "HIGH";
        else
            speed = "NO  ";
        LUSBD_PRINT("[%u] [%u] %s      %2u         %2u         %3u\n",
            lusbd_cfg_list[i].dev_idx,
            lusbd_cfg_list[i].cfg_val,
            speed,
            lusbd_cfg_list[i].max_in_ep_num,
            lusbd_cfg_list[i].max_out_ep_num,
            len);
    }
    LUSBD_PRINT("========================================================\n");
}


static int lusbd_update_dev_desc(unsigned int dev_idx)
{
    lusbd_dev_t *dev;
    unsigned int is_hs = 0;
    unsigned int count;
    int i;

    dev = &lusbd_dev_list[dev_idx];

#if LUSBD_MAX_SPEED == LUSBD_HIGH_SPEED
    is_hs = (LUSBD_HIGH_SPEED == dev->enum_speed);
#endif
    count = 0;
    for (i = 0; i < cfg_count; i++)
    {
        if (   (lusbd_cfg_list[i].dev_idx == dev_idx)
            && (lusbd_cfg_list[i].is_hs == is_hs))
            count++;
    }
    dev->dev_desc.bLength = sizeof(usb_device_descriptor_t);
    dev->dev_desc.bDescriptorType = USB_DESC_TYPE_DEVICE;
    dev->dev_desc.bcdUSB_l = 0x00;
    dev->dev_desc.bcdUSB_h = 0x02;
    dev->dev_desc.bDeviceClass = IAD_DEV_CLASS;
    dev->dev_desc.bDeviceSubClass = IAD_DEV_SUBCLASS;
    dev->dev_desc.bDeviceProtocol = IAD_DEV_PROTOCOL;
    dev->dev_desc.bMaxPacketSize0 = LUSBD_MAX_PACKET_SIZE_EP0;
    dev->dev_desc.bNumConfigurations = count;

#if LUSBD_MAX_SPEED == LUSBD_HIGH_SPEED
    is_hs = !(LUSBD_HIGH_SPEED == dev->enum_speed);

    count = 0;
    for (i = 0; i < cfg_count; i++)
    {
        if (   (lusbd_cfg_list[i].dev_idx == dev_idx)
            && (lusbd_cfg_list[i].is_hs == is_hs))
            count++;
    }
    dev->dev_qualifier_desc.bLength = sizeof(usb_device_qualifier_descriptor_t);
    dev->dev_qualifier_desc.bDescriptorType = USB_DESC_TYPE_DEVICE_QUALIFIER;
    dev->dev_qualifier_desc.bcdUSB_l = 0x00;
    dev->dev_qualifier_desc.bcdUSB_h = 0x02;
    dev->dev_qualifier_desc.bDeviceClass = IAD_DEV_CLASS;
    dev->dev_qualifier_desc.bDeviceSubClass = IAD_DEV_SUBCLASS;
    dev->dev_qualifier_desc.bDeviceProtocol = IAD_DEV_PROTOCOL;
    dev->dev_qualifier_desc.bMaxPacketSize0 = LUSBD_MAX_PACKET_SIZE_EP0;
    dev->dev_qualifier_desc.bNumConfigurations = count;
    dev->dev_qualifier_desc.bReserved = 0;
#endif
    return 0;
}

static void lusbd_notify_classes_deactivate(unsigned int dev_idx)
{
    lusbd_dev_t *dev;
    unsigned int cfg_idx;
    int i;

    dev = &lusbd_dev_list[dev_idx];
    cfg_idx = dev->cur_cfg_idx;

    for (i = 0; i < class_count; i++)
    {
        if (cfg_idx != lusbd_class_list[i].cfg_idx)
            continue;
        if (lusbd_class_list[i].cb && lusbd_class_list[i].cb->deactivate)
            lusbd_class_list[i].cb->deactivate(dev_idx, lusbd_class_list[i].class_data);
    }
}

static int lusbd_activate_cfg(unsigned int dev_idx, unsigned int cfg_val)
{
    unsigned int is_hs = 0;
    unsigned int cur_cfg_idx;
    lusbd_dev_t *dev;
    lusbd_if_t *from_if;
    lusbd_if_t *to_if;
    lusbd_ep_t *from_ep;
    lusbd_ep_t *to_ep;
    unsigned int max_out_ep_num = 0;
    unsigned int max_in_ep_num = 0;
    unsigned int max_alt_mps;
    int i;


    dev = &lusbd_dev_list[dev_idx];

    if (0 == cfg_val)
    {
        memset(&dev->active_ifs, 0, sizeof(dev->active_ifs));
        memset(&dev->active_eps, 0, sizeof(dev->active_eps));
        dev->cur_cfg_val = 0;
        dev->cur_cfg_idx = 0;
        return 0;
    }

#if LUSBD_MAX_SPEED == LUSBD_HIGH_SPEED
    is_hs = (LUSBD_HIGH_SPEED == dev->enum_speed);
#endif

    for (i = 0; i < cfg_count; i++)
    {
        if (   (lusbd_cfg_list[i].dev_idx == dev_idx)
            && (lusbd_cfg_list[i].cfg_val == cfg_val)
            && (lusbd_cfg_list[i].is_hs == is_hs))
        {
            cur_cfg_idx = i;
            break;
        }
    }
    if (cfg_count <= i)
    {
        LUSBD_ERROR("Failed to activate config, dev:%u cfg_val:%u hs:%u not found", dev_idx, cfg_val, is_hs);
        return -1;
    }

    memset(&dev->active_ifs, 0, sizeof(dev->active_ifs));
    memset(&dev->active_eps, 0, sizeof(dev->active_eps));
    dev->cur_cfg_val = 0;
    dev->cur_cfg_idx = 0;

    /* activate interface */
    for (i = 0; i < if_count; i++)
    {
        if (cur_cfg_idx != lusbd_class_list[lusbd_if_list[i].class_idx].cfg_idx)
            continue;

        from_if = &lusbd_if_list[i];
        to_if = &dev->active_ifs[from_if->if_num];
        if (to_if->used)
        {
            memset(&dev->active_ifs, 0, sizeof(dev->active_ifs));
            LUSBD_ERROR("Failed to activate config, duplicate if_num(%u), dev:%u cfg_val:%u hs:%u", from_if->if_num, dev_idx, cfg_val, is_hs);
            return -1;
        }
        *to_if = *from_if;
    }

    /* activate endpoint */
    for (i = 0; i < ep_count; i++)
    {
        if (cur_cfg_idx != lusbd_class_list[lusbd_ep_list[i].class_idx].cfg_idx)
            continue;

        from_ep = &lusbd_ep_list[i];
        to_ep = &dev->active_eps[from_ep->dir][from_ep->ep_num];

        if (0 == to_ep->used)
        {
            *to_ep = *from_ep;
        }
        else
        {
            if (to_ep->alt_num == from_ep->alt_num)
            {
                memset(&dev->active_ifs, 0, sizeof(dev->active_ifs));
                memset(&dev->active_eps, 0, sizeof(dev->active_eps));
                LUSBD_ERROR("Failed to activate config, duplicate dir(%u) ep_num(%u), dev:%u cfg_val:%u hs:%u alt_num:%u",
                            from_ep->dir, from_ep->ep_num, dev_idx, cfg_val, is_hs, from_ep->alt_num);
                return -1;
            }
            if (to_ep->max_alt_mps < from_ep->max_packet_size)
                max_alt_mps = from_ep->max_packet_size;
            else
                max_alt_mps = to_ep->max_alt_mps;
            if (to_ep->alt_num > from_ep->alt_num)
                *to_ep = *from_ep;
            to_ep->max_alt_mps = max_alt_mps;
        }

        if (LUSBD_DIR_IN == from_ep->dir)
        {
            if (max_in_ep_num < from_ep->ep_num)
                max_in_ep_num = from_ep->ep_num;
        }
        else
        {
            if (max_out_ep_num < from_ep->ep_num)
                max_out_ep_num = from_ep->ep_num;
        }
    }
    dev->active_eps[0][0].used = 1;
    dev->active_eps[0][0].dir = LUSBD_DIR_OUT;
    dev->active_eps[0][0].type = LUSBD_EP_TYPE_CONTROL;
    dev->active_eps[0][0].max_packet_size = LUSBD_MAX_PACKET_SIZE_EP0;
    dev->active_eps[0][0].max_alt_mps = LUSBD_MAX_PACKET_SIZE_EP0;
    dev->active_eps[1][0].used = 1;
    dev->active_eps[1][0].dir = LUSBD_DIR_IN;
    dev->active_eps[1][0].type = LUSBD_EP_TYPE_CONTROL;
    dev->active_eps[1][0].max_packet_size = LUSBD_MAX_PACKET_SIZE_EP0;
    dev->active_eps[1][0].max_alt_mps = LUSBD_MAX_PACKET_SIZE_EP0;
    if (lusbd_hal_init_eps(dev_idx, dev->active_eps[0], max_out_ep_num + 1, dev->active_eps[1], max_in_ep_num + 1))
    {
        memset(&dev->active_ifs, 0, sizeof(dev->active_ifs));
        memset(&dev->active_eps, 0, sizeof(dev->active_eps));
        LUSBD_ERROR("Failed to activate config, init eps failed");
        return -1;
    }

    dev->cur_cfg_val = cfg_val;
    dev->cur_cfg_idx = cur_cfg_idx;
    dev->self_powered = lusbd_cfg_list[cur_cfg_idx].cfg_desc.bmAttributes_self_powered;

    /* notify class */
    for (i = 0; i < class_count; i++)
    {
        if (cur_cfg_idx != lusbd_class_list[i].cfg_idx)
            continue;

        if (lusbd_class_list[i].cb && lusbd_class_list[i].cb->activate)
            lusbd_class_list[i].cb->activate(dev_idx, is_hs, lusbd_class_list[i].class_data);
    }

    return 0;
}

static int lusbd_activate_ep(unsigned int dev_idx, unsigned int if_num, unsigned int alt_num)
{
    return 0;
}




int lusbd_get_device_info(unsigned int dev_idx, lusbd_device_info_t *info)
{
    lusbd_dev_t *dev;

    if ((LUSBD_DEV_COUNT <= dev_idx) || (NULL == info))
        return -1;
    dev = &lusbd_dev_list[dev_idx];

    info->dev_state = dev->dev_state;
    info->suspend = dev->suspend;
    info->enum_speed = dev->enum_speed;
    info->cur_cfg_val = dev->cur_cfg_val;
    return 0;
}

int lusbd_start_device(unsigned int dev_idx)
{
    if (LUSBD_DEV_COUNT <= dev_idx)
        return -1;

    if (lusbd_hal_start_device(dev_idx))
        return -1;
    return 0;
}

int lusbd_stop_device(unsigned int dev_idx)
{
    if (LUSBD_DEV_COUNT <= dev_idx)
        return -1;

    if (lusbd_hal_stop_device(dev_idx))
        return -1;
    lusbd_dev_list[dev_idx].dev_state = USB_STATE_NOTATTACHED;
    return 0;
}

int lusbd_ep_set_halt(unsigned int dev_idx, unsigned int dir, unsigned int num)
{
    lusbd_dev_t *dev;

    if (LUSBD_DEV_COUNT <= dev_idx)
        return -1;
    if (dir)
    {
        if (LUSBD_IN_EP_MAX_NUM < num)
            return -1;
    }
    else
    {
        if (LUSBD_OUT_EP_MAX_NUM < num)
            return -1;
    }
    dev = &lusbd_dev_list[dev_idx];

    if (0 == num)
    {
        lusbd_hal_ep_set_halt(dev_idx, 0, 0);
        lusbd_hal_ep_set_halt(dev_idx, 1, 0);
        dev->ep0_tx_buf = NULL;
        dev->ep0_tx_len = 0;
        dev->control_out_cb = NULL;
        dev->is_rx_status = 0;
    }
    else
    {
        dir = !!dir;
        if (0 == dev->active_eps[dir][num].used)
        {
            LUSBD_ERROR("Failed to halt endpoint, dev(%u) dir(%u) num(%u) invalid", dev_idx, dir, num);
            return -1;
        }
        if (lusbd_hal_ep_set_halt(dev_idx, dir, num))
        {
            LUSBD_ERROR("Halt endpoint failed, dev:%u dir:%u num:%u", dev_idx, dir, num);
            return -1;
        }
        dev->active_eps[dir][num].halt = 1;
    }
    return 0;
}

int lusbd_ep_clear_halt(unsigned int dev_idx, unsigned int dir, unsigned int num)
{
    lusbd_dev_t *dev;

    if (LUSBD_DEV_COUNT <= dev_idx)
        return -1;
    if (dir)
    {
        if (LUSBD_IN_EP_MAX_NUM < num)
            return -1;
    }
    else
    {
        if (LUSBD_OUT_EP_MAX_NUM < num)
            return -1;
    }
    dev = &lusbd_dev_list[dev_idx];

    if (0 == num)
    {
        return -1;
    }
    else
    {
        dir = !!dir;
        if (0 == dev->active_eps[dir][num].used)
        {
            LUSBD_ERROR("Failed to clear endpoint halt, dev(%u) dir(%u) num(%u) invalid", dev_idx, dir, num);
            return -1;
        }
        if (lusbd_hal_ep_clear_halt(dev_idx, dir, num))
        {
            LUSBD_ERROR("Clear endpoint halt failed, dev:%u dir:%u num:%u", dev_idx, dir, num);
            return -1;
        }
        dev->active_eps[dir][num].halt = 0;
    }
    return 0;
}

int lusbd_ep_tx(unsigned int dev_idx, unsigned int num, const void *buf, unsigned int len, unsigned int need_zlp)
{
    lusbd_dev_t *dev;
    lusbd_ep_t *ep;

    if ((LUSBD_DEV_COUNT <= dev_idx) || ((NULL == buf) && len) || (((uintptr_t)buf) & 0x03))
        return -1;
    dev = &lusbd_dev_list[dev_idx];

    if (num)
    {
        if (LUSBD_IN_EP_MAX_NUM < num)
            return -1;
        ep = &dev->active_eps[1][num];
        if (0 == ep->used)
            return -1;

        if ((0 == len) || (len % ep->max_packet_size))
            ep->need_zlp = 0;
        else
            ep->need_zlp = need_zlp;
        return lusbd_hal_ep_tx(dev_idx, num, buf, len);
    }
    else
    {
        if (LUSBD_MAX_PACKET_SIZE_EP0 <= len)
        {
            dev->need_zlp = need_zlp;
            dev->ep0_tx_len = len;
            dev->ep0_tx_buf = buf;
            return lusbd_hal_ep_tx(dev_idx, 0, buf, LUSBD_MAX_PACKET_SIZE_EP0);
        }
        else if (len)
        {
            dev->need_zlp = 0;
            dev->ep0_tx_len = 0;
            dev->ep0_tx_buf = NULL;
            return lusbd_hal_ep_tx(dev_idx, 0, buf, len);
        }
        else
        {
            dev->ep0_tx_buf = NULL;
            dev->ep0_tx_len = 0;
            dev->need_zlp = 0;
            return lusbd_hal_ep_tx(dev_idx, 0, NULL, 0);
        }
    }
}

int lusbd_ep_rx(unsigned int dev_idx, unsigned int num, void *buf, unsigned int len)
{
    if ((LUSBD_DEV_COUNT <= dev_idx) || ((NULL == buf) && len) || (((uintptr_t)buf) & 0x03))
        return -1;
    return lusbd_hal_ep_rx(dev_idx, num, buf, len);
}

int lusbd_control_tx_status(unsigned int dev_idx)
{
    lusbd_dev_t *dev;

    if (LUSBD_DEV_COUNT <= dev_idx)
        return -1;
    dev = &lusbd_dev_list[dev_idx];

    dev->ep0_tx_buf = NULL;
    dev->ep0_tx_len = 0;
    dev->need_zlp = 0;
    dev->control_out_cb = NULL;
    return lusbd_hal_ep_tx(dev_idx, 0, NULL, 0);
}

int lusbd_control_rx_status(unsigned int dev_idx)
{
    if (LUSBD_DEV_COUNT <= dev_idx)
        return -1;
    lusbd_dev_list[dev_idx].is_rx_status = 1;
    return lusbd_hal_ep_rx(dev_idx, 0, NULL, 0);
}

int lusbd_control_rx(unsigned int dev_idx, void *buf, unsigned int len, lusbd_control_out_cb_t cb, void *arg, void *class_data)
{
    lusbd_dev_t *dev;

    if ((LUSBD_DEV_COUNT <= dev_idx) || (NULL == buf) || (((uintptr_t)buf) & 0x03) || (0 == len) || (NULL == cb))
        return -1;
    dev = &lusbd_dev_list[dev_idx];

    dev->is_rx_status = 0;
    dev->control_out_class_data = class_data;
    dev->control_out_arg = arg;
    dev->control_out_cb = cb;
    return lusbd_hal_ep_rx(dev_idx, 0, buf, len);
}




void lusbd_connect_handler(unsigned int dev_idx)
{
    if (LUSBD_DEV_COUNT <= dev_idx)
        return;
    if (USB_STATE_ATTACHED > lusbd_dev_list[dev_idx].dev_state)
        lusbd_dev_list[dev_idx].dev_state = USB_STATE_ATTACHED;
    LUSBD_INFO("Dev(%u) connected", dev_idx);
    if (lusbd_dev_list[dev_idx].user_cb)
        lusbd_dev_list[dev_idx].user_cb(dev_idx, LUSBD_USER_EVENT_CONNECTED);
}

void lusbd_disconnect_handler(unsigned int dev_idx)
{
    if (LUSBD_DEV_COUNT <= dev_idx)
        return;
    lusbd_dev_list[dev_idx].dev_state = USB_STATE_NOTATTACHED;
    LUSBD_INFO("Dev(%u) disconnected", dev_idx);

    lusbd_notify_classes_deactivate(dev_idx);
    if (lusbd_dev_list[dev_idx].user_cb)
        lusbd_dev_list[dev_idx].user_cb(dev_idx, LUSBD_USER_EVENT_DISCONNECTED);
}

void lusbd_suspend_handler(unsigned int dev_idx)
{
    if (LUSBD_DEV_COUNT <= dev_idx)
        return;
    lusbd_dev_list[dev_idx].suspend = 1;
    LUSBD_DEBUG("Dev(%u) suspended", dev_idx);
    if (lusbd_dev_list[dev_idx].user_cb)
        lusbd_dev_list[dev_idx].user_cb(dev_idx, LUSBD_USER_EVENT_SUSPENDED);
}

void lusbd_resume_handler(unsigned int dev_idx)
{
    if (LUSBD_DEV_COUNT <= dev_idx)
        return;
    lusbd_dev_list[dev_idx].suspend = 0;
    LUSBD_DEBUG("Dev(%u) resumed", dev_idx);
    if (lusbd_dev_list[dev_idx].user_cb)
        lusbd_dev_list[dev_idx].user_cb(dev_idx, LUSBD_USER_EVENT_RESUMED);
}

void lusbd_reset_handler(unsigned int dev_idx, unsigned int speed)
{
    lusbd_dev_t *dev;

    if (LUSBD_DEV_COUNT <= dev_idx)
        return;
    dev = &lusbd_dev_list[dev_idx];

    lusbd_notify_classes_deactivate(dev_idx);
    dev->dev_state = USB_STATE_DEFAULT;
    dev->suspend = 0;
    dev->enum_speed = speed;
    dev->cur_cfg_val = 0;
    dev->cur_cfg_idx = 0;
    dev->ep0_tx_buf = NULL;
    dev->ep0_tx_len = 0;
    dev->need_zlp = 0;
    dev->control_out_cb = NULL;
    dev->is_rx_status = 0;
    dev->remote_wakeup = 0;
    lusbd_update_dev_desc(dev_idx);

    lusbd_activate_cfg(dev_idx, 0);
    lusbd_hal_ep_open(dev_idx, 0, 0, LUSBD_MAX_PACKET_SIZE_EP0, USB_EP_DESC_TRANSFER_TYPE_CONTROL);
    lusbd_hal_ep_open(dev_idx, 1, 0, LUSBD_MAX_PACKET_SIZE_EP0, USB_EP_DESC_TRANSFER_TYPE_CONTROL);

    LUSBD_DEBUG("Dev(%u) reset, speed:%u", dev_idx, speed);
}

static void setup_get_status(unsigned int dev_idx, const usb_setup_data_t *setup_data)
{
    lusbd_dev_t *dev;
    lusbd_ep_t *ep;
    unsigned int dir;
    unsigned int ep_num;


    if (   (1 != setup_data->bmRequestType_direction)
        || (0 != setup_data->bmRequestType_type)
        || (0 != setup_data->wValue_l)
        || (0 != setup_data->wValue_h)
        || (2 != setup_data->wLength_l)
        || (0 != setup_data->wLength_h))
    {
        lusbd_ep_set_halt(dev_idx, 0, 0);
        LUSBD_INFO("Dev(%u) setup get_status invalid, bmRequestType:0x%02x wValue:0x%02x%02x wLength:0x%02x%02x, ep0 halted",
                   dev_idx, setup_data->bmRequestType, setup_data->wValue_h, setup_data->wValue_l, setup_data->wLength_h, setup_data->wLength_l);
        return;
    }
    dev = &lusbd_dev_list[dev_idx];

    if (USB_REQ_RECIPIENT_DEVICE == setup_data->bmRequestType_recipient)
    {
        if (   (0 != setup_data->wIndex_l)
            || (0 != setup_data->wIndex_h))
        {
            lusbd_ep_set_halt(dev_idx, 0, 0);
            LUSBD_INFO("Dev(%u) setup get_status device invalid, wIndex:0x%02x%02x, ep0 halted", dev_idx, setup_data->wIndex_h, setup_data->wIndex_l);
            return;
        }
        memset(&dev->tx_buf.device_status, 0, sizeof(dev->tx_buf.device_status));
        dev->tx_buf.device_status.RemoteWakeup = dev->remote_wakeup;
        dev->tx_buf.device_status.SelfPowered = dev->self_powered;
        lusbd_ep_tx(dev_idx, 0, &dev->tx_buf.device_status, sizeof(dev->tx_buf.device_status), 0);
        lusbd_control_rx_status(dev_idx);
        LUSBD_DEBUG("Dev(%u) setup get_status device, remote_wakeup:%u self_powered:%u", dev_idx, dev->remote_wakeup, dev->self_powered);
    }
    else if (USB_REQ_RECIPIENT_INTERFACE == setup_data->bmRequestType_recipient)
    {
        if (USB_STATE_CONFIGURED > dev->dev_state)
        {
            lusbd_ep_set_halt(dev_idx, 0, 0);
            LUSBD_INFO("Dev(%u) setup get_status interface unsupported in unconfigured state(%u), ep0 halted", dev_idx, dev->dev_state);
            return;
        }
        dev->tx_buf.interface_status[0] = 0;
        dev->tx_buf.interface_status[1] = 0;
        lusbd_ep_tx(dev_idx, 0, dev->tx_buf.interface_status, sizeof(dev->tx_buf.interface_status), 0);
        lusbd_control_rx_status(dev_idx);
        LUSBD_DEBUG("Dev(%u) setup get_status interface", dev_idx);
    }
    else if (USB_REQ_RECIPIENT_ENDPOINT == setup_data->bmRequestType_recipient)
    {
        dir = !!(setup_data->wIndex_l & 0x80);
        ep_num = setup_data->wIndex_l & 0x0f;
        if (ep_num && (USB_STATE_CONFIGURED > dev->dev_state))
        {
            lusbd_ep_set_halt(dev_idx, 0, 0);
            LUSBD_INFO("Dev(%u) setup get_status endpoint unsupported in dev_state(%u), dir:%u num:%u, ep0 halted",
                       dev_idx, dev->dev_state, dir, ep_num);
            return;
        }
        if (dir)
        {
            if (LUSBD_IN_EP_MAX_NUM < ep_num)
            {
                lusbd_ep_set_halt(dev_idx, 0, 0);
                LUSBD_INFO("Dev(%u) setup get_status endpoint invalid, dir(1) num(%u) exceeds the max(%u), ep0 halted",
                           dev_idx, ep_num, LUSBD_IN_EP_MAX_NUM);
                return;
            }
        }
        else
        {
            if (LUSBD_OUT_EP_MAX_NUM < ep_num)
            {
                lusbd_ep_set_halt(dev_idx, 0, 0);
                LUSBD_INFO("Dev(%u) setup get_status endpoint invalid, dir(0) num(%u) exceeds the max(%u), ep0 halted", dev_idx, ep_num, LUSBD_OUT_EP_MAX_NUM);
                return;
            }
        }
        ep = &dev->active_eps[dir][ep_num];
        if (ep_num && (0 == ep->used))
        {
            lusbd_ep_set_halt(dev_idx, 0, 0);
            LUSBD_INFO("Dev(%u) setup get_status endpoint invalid, dir:%u num:%u, ep0 halted", dev_idx, dir, ep_num);
            return;
        }
        memset(&dev->tx_buf.endpoint_status, 0, sizeof(dev->tx_buf.endpoint_status));
        dev->tx_buf.endpoint_status.Halt = ep->halt;
        lusbd_ep_tx(dev_idx, 0, &dev->tx_buf.endpoint_status, sizeof(dev->tx_buf.endpoint_status), 0);
        lusbd_control_rx_status(dev_idx);
        LUSBD_DEBUG("Dev(%u) setup get_status endpoint, dir:%u num:%u halt:%u", dev_idx, dir, ep_num, ep->halt);
    }
}

static void setup_clear_feature(unsigned int dev_idx, const usb_setup_data_t *setup_data)
{
    lusbd_dev_t *dev;
    unsigned int feature;
    unsigned int dir;
    unsigned int ep_num;


    if (   (0 != setup_data->bmRequestType_direction)
        || (0 != setup_data->bmRequestType_type)
        || (0 != setup_data->wLength_l)
        || (0 != setup_data->wLength_h))
    {
        lusbd_ep_set_halt(dev_idx, 0, 0);
        LUSBD_INFO("Dev(%u) setup clear_feature invalid, bmRequestType:0x%02x wLength:0x%02x%02x, ep0 halted",
                   dev_idx, setup_data->bmRequestType, setup_data->wLength_h, setup_data->wLength_l);
        return;
    }
    dev = &lusbd_dev_list[dev_idx];
    feature = (setup_data->wValue_h << 8) | setup_data->wValue_l;

    if ((USB_REQ_FEATURE_ENDPOINT_HALT == feature) && (USB_REQ_RECIPIENT_ENDPOINT == setup_data->bmRequestType_recipient))
    {
        dir = !!(setup_data->wIndex_l & 0x80);
        ep_num = setup_data->wIndex_l & 0x0f;
        if (ep_num && (USB_STATE_CONFIGURED > dev->dev_state))
        {
            lusbd_ep_set_halt(dev_idx, 0, 0);
            LUSBD_INFO("Dev(%u) setup clear_feature endpoint_halt unsupported in dev_state(%u), dir:%u num:%u, ep0 halted",
                       dev_idx, dev->dev_state, dir, ep_num);
            return;
        }
        if (lusbd_ep_clear_halt(dev_idx, dir, ep_num))
        {
            lusbd_ep_set_halt(dev_idx, 0, 0);
            LUSBD_INFO("Dev(%u) setup set_feature endpoint_halt failed, dir:%u num:%u, ep0 halted", dev_idx, dir, ep_num);
            return;
        }
        lusbd_control_tx_status(dev_idx);
        LUSBD_INFO("Dev(%u) setup clear_feature endpoint_halt, dir:%u num:%u", dev_idx, dir, ep_num);
    }
    else if ((USB_REQ_FEATURE_DEVICE_REMOTE_WAKEUP == feature) && (USB_REQ_RECIPIENT_DEVICE == setup_data->bmRequestType_recipient))
    {
        lusbd_control_tx_status(dev_idx);
        dev->remote_wakeup = 0;
        LUSBD_INFO("Dev(%u) setup clear_feature remote_wakeup", dev_idx);
    }
    else if ((USB_REQ_FEATURE_TEST_MODE == feature) && (USB_REQ_RECIPIENT_DEVICE == setup_data->bmRequestType_recipient))
    {
        lusbd_ep_set_halt(dev_idx, 0, 0);
        LUSBD_INFO("Dev(%u) setup clear_feature test_mode unsupported, ep0 halted", dev_idx);
        return;
    }
    else
    {
        lusbd_ep_set_halt(dev_idx, 0, 0);
        LUSBD_INFO("Dev(%u) setup clear_feature invalid, bmRequestType:0x%02x feature:%u, ep0 halted", dev_idx, setup_data->bmRequestType, feature);
        return;
    }
}

static void setup_set_feature(unsigned int dev_idx, const usb_setup_data_t *setup_data)
{
    lusbd_dev_t *dev;
    unsigned int feature;
    unsigned int dir;
    unsigned int ep_num;


    if (   (0 != setup_data->bmRequestType_direction)
        || (0 != setup_data->bmRequestType_type)
        || (0 != setup_data->wLength_l)
        || (0 != setup_data->wLength_h))
    {
        lusbd_ep_set_halt(dev_idx, 0, 0);
        LUSBD_INFO("Dev(%u) setup set_feature invalid, bmRequestType:0x%02x wLength:0x%02x%02x, ep0 halted",
                   dev_idx, setup_data->bmRequestType, setup_data->wLength_h, setup_data->wLength_l);
        return;
    }
    feature = (setup_data->wValue_h << 8) | setup_data->wValue_l;
    dev = &lusbd_dev_list[dev_idx];

    if ((USB_REQ_FEATURE_ENDPOINT_HALT == feature) && (USB_REQ_RECIPIENT_ENDPOINT == setup_data->bmRequestType_recipient))
    {
        dir = !!(setup_data->wIndex_l & 0x80);
        ep_num = setup_data->wIndex_l & 0x0f;
        if (ep_num && (USB_STATE_CONFIGURED > dev->dev_state))
        {
            lusbd_ep_set_halt(dev_idx, 0, 0);
            LUSBD_INFO("Dev(%u) setup set_feature endpoint_halt unsupported in dev_state(%u), dir:%u num:%u, ep0 halted",
                       dev_idx, dev->dev_state, dir, ep_num);
            return;
        }
        if (lusbd_ep_set_halt(dev_idx, dir, ep_num))
        {
            lusbd_ep_set_halt(dev_idx, 0, 0);
            LUSBD_INFO("Dev(%u) setup set_feature endpoint_halt failed, dir:%u num:%u, ep0 halted", dev_idx, dir, ep_num);
            return;
        }
        lusbd_control_tx_status(dev_idx);
        LUSBD_INFO("Dev(%u) setup set_feature endpoint_halt, dir:%u num:%u", dev_idx, dir, ep_num);
    }
    else if ((USB_REQ_FEATURE_DEVICE_REMOTE_WAKEUP == feature) && (USB_REQ_RECIPIENT_DEVICE == setup_data->bmRequestType_recipient))
    {
        lusbd_control_tx_status(dev_idx);
        dev->remote_wakeup = 1;
        LUSBD_INFO("Dev(%u) setup set_feature remote_wakeup", dev_idx);
    }
    else if ((USB_REQ_FEATURE_TEST_MODE == feature) && (USB_REQ_RECIPIENT_DEVICE == setup_data->bmRequestType_recipient))
    {
        if (0 != setup_data->wIndex_l)
        {
            lusbd_ep_set_halt(dev_idx, 0, 0);
            LUSBD_INFO("Dev(%u) setup set_feature test_mode invalid, wIndex:0x%02x%02x, ep0 halted", dev_idx, setup_data->wIndex_h, setup_data->wIndex_l);
            return;
        }
        dev->test_mode = setup_data->wIndex_h;
        dev->test_mode_state = 1;
        lusbd_control_tx_status(dev_idx);
        LUSBD_INFO("Dev(%u) setup set_feature test_mode %u", dev_idx, dev->test_mode);
    }
    else
    {
        lusbd_ep_set_halt(dev_idx, 0, 0);
        LUSBD_INFO("Dev(%u) setup set_feature invalid, bmRequestType:0x%02x feature:%u, ep0 halted", dev_idx, setup_data->bmRequestType, feature);
        return;
    }
}

static void setup_set_address(unsigned int dev_idx, const usb_setup_data_t *setup_data)
{
    unsigned int addr;

    if (   (0 != setup_data->bmRequestType)
        || (0 != setup_data->wIndex_l)
        || (0 != setup_data->wIndex_h)
        || (0 != setup_data->wLength_l)
        || (0 != setup_data->wLength_h))
    {
        lusbd_ep_set_halt(dev_idx, 0, 0);
        LUSBD_INFO("Dev(%u) setup set_address invalid, bmRequestType:0x%02x wIndex:0x%02x%02x wLength:0x%02x%02x, ep0 halted",
                   dev_idx, setup_data->bmRequestType, setup_data->wIndex_h, setup_data->wIndex_l, setup_data->wLength_h, setup_data->wLength_l);
        return;
    }

    addr = (setup_data->wValue_h << 8) | setup_data->wValue_l;
    if (127 < addr)
    {
        lusbd_ep_set_halt(dev_idx, 0, 0);
        LUSBD_INFO("Dev(%u) setup set_address invalid, addr:%u, ep0 halted", dev_idx, addr);
        return;
    }

    lusbd_hal_set_address(dev_idx, addr);
    lusbd_control_tx_status(dev_idx);
    if (0 == addr)
    {
        lusbd_dev_list[dev_idx].dev_state = USB_STATE_DEFAULT;
        LUSBD_INFO("Dev(%u) setup set_address 0, go to default state", dev_idx);
    }
    else
    {
        lusbd_dev_list[dev_idx].dev_state = USB_STATE_ADDRESS;
        LUSBD_DEBUG("Dev(%u) setup set_address %u", dev_idx, addr);
    }
}

static void get_device_desc(unsigned int dev_idx, const usb_setup_data_t *setup_data)
{
    unsigned int data_len;

    data_len = (setup_data->wLength_h << 8) | setup_data->wLength_l;

    if (sizeof(lusbd_dev_list[dev_idx].dev_desc) < data_len)
        lusbd_ep_tx(dev_idx, 0, &lusbd_dev_list[dev_idx].dev_desc, sizeof(lusbd_dev_list[dev_idx].dev_desc), 1);
    else
        lusbd_ep_tx(dev_idx, 0, &lusbd_dev_list[dev_idx].dev_desc, data_len, 0);
    lusbd_control_rx_status(dev_idx);
    LUSBD_DEBUG("Dev(%u) setup get_device_descriptor, len:%u", dev_idx, data_len);
}

static void get_config_desc(unsigned int dev_idx, const usb_setup_data_t *setup_data, unsigned int other_speed)
{
    unsigned int desc_idx;
    unsigned int data_len;
    unsigned int is_hs = 0;
    lusbd_cfg_t *cfg;
    unsigned int total_length;
    int i;

#if LUSBD_MAX_SPEED != LUSBD_HIGH_SPEED
    if (other_speed)
    {
        lusbd_ep_set_halt(dev_idx, 0, 0);
        LUSBD_INFO("Dev(%u) setup get_other_config_descriptor unsupported, ep0 halted", dev_idx);
        return;
    }
#endif

    desc_idx = setup_data->wValue_l;
    data_len = (setup_data->wLength_h << 8) | setup_data->wLength_l;
#if LUSBD_MAX_SPEED == LUSBD_HIGH_SPEED
    is_hs = (LUSBD_HIGH_SPEED == lusbd_dev_list[dev_idx].enum_speed);
    if (other_speed)
        is_hs = !is_hs;
#endif
    for (i = 0; i < cfg_count; i++)
    {
        if (   (lusbd_cfg_list[i].dev_idx == dev_idx)
            && (lusbd_cfg_list[i].is_hs == is_hs))
        {
            if (0 == desc_idx)
                break;
            else
                desc_idx--;
        }
    }
    if (cfg_count <= i)
    {
        lusbd_ep_set_halt(dev_idx, 0, 0);
        LUSBD_INFO("Dev(%u) setup get_config_descriptor invalid, config desc index:%u, ep0 halted", dev_idx, desc_idx);
        return;
    }
    cfg = &lusbd_cfg_list[i];
#if LUSBD_MAX_SPEED == LUSBD_HIGH_SPEED
    if (other_speed)
        cfg->cfg_desc.bDescriptorType = USB_DESC_TYPE_OTHER_SPEED_CONFIGURATION;
    else
        cfg->cfg_desc.bDescriptorType = USB_DESC_TYPE_CONFIGURATION;
#endif
    total_length = (cfg->cfg_desc.wTotalLength_h << 8) | cfg->cfg_desc.wTotalLength_l;
    if (total_length < data_len)
        lusbd_ep_tx(dev_idx, 0, cfg->cfg_desc_buf, total_length, 1);
    else
        lusbd_ep_tx(dev_idx, 0, cfg->cfg_desc_buf, data_len, 0);
    lusbd_control_rx_status(dev_idx);

    LUSBD_DEBUG("Dev(%u) setup get_config_descriptor, index:%u len:%u desc_len:%u", dev_idx, desc_idx, data_len, total_length);
}

static void get_string_desc(unsigned int dev_idx, const usb_setup_data_t *setup_data)
{
    unsigned int desc_idx;
    unsigned int data_len;
    unsigned int lang_id;
    const uint8_t *buf;
    unsigned int str_len;

    desc_idx = setup_data->wValue_l;
    data_len = (setup_data->wLength_h << 8) | setup_data->wLength_l;
    lang_id = (setup_data->wIndex_h << 8) | setup_data->wIndex_l;

    if (str_count < desc_idx)
    {
        lusbd_ep_set_halt(dev_idx, 0, 0);
        LUSBD_INFO("Dev(%u) setup get_string_descriptor invalid, string desc index:%u, ep0 halted", dev_idx, desc_idx);
        return;
    }
    if (desc_idx)
    {
        buf = lusbd_str_list[desc_idx - 1].buf;
        str_len = lusbd_str_list[desc_idx - 1].len;
    }
    else
    {
        buf = lang_id_desc;
        str_len = sizeof(lang_id_desc);
    }
    if (str_len < data_len)
        lusbd_ep_tx(dev_idx, 0, buf, str_len, 1);
    else
        lusbd_ep_tx(dev_idx, 0, buf, data_len, 0);
    lusbd_control_rx_status(dev_idx);

    LUSBD_DEBUG("Dev(%u) setup get_string_descriptor, lang:0x%x index:%u len:%u str_len:%u", dev_idx, lang_id, desc_idx, data_len, str_len);
}

static void get_device_qualifier_desc(unsigned int dev_idx, const usb_setup_data_t *setup_data)
{
#if LUSBD_MAX_SPEED != LUSBD_HIGH_SPEED
    lusbd_ep_set_halt(dev_idx, 0, 0);
    LUSBD_INFO("Dev(%u) setup get_device_qualifier_descriptor unsupported, ep0 halted", dev_idx);
    return;
#else
    unsigned int data_len;

    data_len = (setup_data->wLength_h << 8) | setup_data->wLength_l;
    if (sizeof(lusbd_dev_list[dev_idx].dev_qualifier_desc) < data_len)
        lusbd_ep_tx(dev_idx, 0, &lusbd_dev_list[dev_idx].dev_qualifier_desc, sizeof(lusbd_dev_list[dev_idx].dev_qualifier_desc), 1);
    else
        lusbd_ep_tx(dev_idx, 0, &lusbd_dev_list[dev_idx].dev_qualifier_desc, data_len, 0);
    lusbd_control_rx_status(dev_idx);
    LUSBD_DEBUG("Dev(%u) setup get_device_qualifier_descriptor, len:%u", dev_idx, data_len);
#endif
}

static void setup_get_descriptor(unsigned int dev_idx, const usb_setup_data_t *setup_data)
{
    unsigned int desc_type;

    if (0x80 != setup_data->bmRequestType)
    {
        lusbd_ep_set_halt(dev_idx, 0, 0);
        LUSBD_INFO("Dev(%u) setup get_descriptor invalid, bmRequestType:0x%02x, ep0 halted", dev_idx, setup_data->bmRequestType);
        return;
    }

    desc_type = setup_data->wValue_h;
    switch (desc_type)
    {
        case USB_DESC_TYPE_DEVICE:
        {
            get_device_desc(dev_idx, setup_data);
            break;
        }
        case USB_DESC_TYPE_CONFIGURATION:
        {
            get_config_desc(dev_idx, setup_data, 0);
            break;
        }
        case USB_DESC_TYPE_STRING:
        {
            get_string_desc(dev_idx, setup_data);
            break;
        }
        case USB_DESC_TYPE_DEVICE_QUALIFIER:
        {
            get_device_qualifier_desc(dev_idx, setup_data);
            break;
        }
        case USB_DESC_TYPE_OTHER_SPEED_CONFIGURATION:
        {
            get_config_desc(dev_idx, setup_data, 1);
            break;
        }
        default:
        {
            lusbd_ep_set_halt(dev_idx, 0, 0);
            LUSBD_INFO("Dev(%u) setup get_descriptor DescriptorType(%u) unsupported, ep0 halted", dev_idx, desc_type);
            return;
        }
    }
}

static void setup_set_descriptor(int dev_idx, const usb_setup_data_t *setup_data)
{
    lusbd_ep_set_halt(dev_idx, 0, 0);
    LUSBD_INFO("Dev(%u) setup set_descriptor unsupported, ep0 halted", dev_idx);
    return;
}

static void setup_get_config(unsigned int dev_idx, const usb_setup_data_t *setup_data)
{
    lusbd_dev_t *dev;

    if (   (0x80 != setup_data->bmRequestType)
        || (0 != setup_data->wValue_l)
        || (0 != setup_data->wValue_h)
        || (0 != setup_data->wIndex_l)
        || (0 != setup_data->wIndex_h)
        || (1 != setup_data->wLength_l)
        || (0 != setup_data->wLength_h))
    {
        lusbd_ep_set_halt(dev_idx, 0, 0);
        LUSBD_INFO("Dev(%u) setup get_config invalid, bmRequestType:0x%02x wValue:0x%02x%02x wIndex:0x%02x%02x wLength:0x%02x%02x, ep0 halted",
                   dev_idx, setup_data->bmRequestType,
                   setup_data->wValue_h, setup_data->wValue_l,
                   setup_data->wIndex_h, setup_data->wIndex_l,
                   setup_data->wLength_h, setup_data->wLength_l);
        return;
    }
    dev = &lusbd_dev_list[dev_idx];

    dev->tx_buf.cfg_val = dev->cur_cfg_val;
    lusbd_ep_tx(dev_idx, 0, &dev->tx_buf.cfg_val, sizeof(dev->tx_buf.cfg_val), 0);
    lusbd_control_rx_status(dev_idx);
    LUSBD_DEBUG("Dev(%u) setup get_config %u", dev_idx, dev->cur_cfg_val);
}

static void setup_set_config(unsigned int dev_idx, const usb_setup_data_t *setup_data)
{
    unsigned int cfg_val;

    if (0 != setup_data->bmRequestType)
    {
        lusbd_ep_set_halt(dev_idx, 0, 0);
        LUSBD_INFO("Dev(%u) setup set_config invalid, bmRequestType:0x%02x, ep0 halted", dev_idx, setup_data->bmRequestType);
        return;
    }

    cfg_val = setup_data->wValue_l;
    if (lusbd_dev_list[dev_idx].cur_cfg_val)
        lusbd_notify_classes_deactivate(dev_idx);
    if (lusbd_activate_cfg(dev_idx, cfg_val))
    {
        lusbd_ep_set_halt(dev_idx, 0, 0);
        return;
    }

    lusbd_control_tx_status(dev_idx);
    if (cfg_val)
        lusbd_dev_list[dev_idx].dev_state = USB_STATE_CONFIGURED;
    else
        lusbd_dev_list[dev_idx].dev_state = USB_STATE_ADDRESS;
    LUSBD_INFO("Dev(%u) setup set_config %u", dev_idx, cfg_val);

    if (cfg_val && lusbd_dev_list[dev_idx].user_cb)
        lusbd_dev_list[dev_idx].user_cb(dev_idx, LUSBD_USER_EVENT_CONFIGURED);
}

static void setup_get_interface(unsigned int dev_idx, const usb_setup_data_t *setup_data)
{
    lusbd_dev_t *dev;
    lusbd_if_t *active_if;

    if (   (0x81 != setup_data->bmRequestType)
        || (0 != setup_data->wValue_l)
        || (0 != setup_data->wValue_h)
        || (1 != setup_data->wLength_l)
        || (0 != setup_data->wLength_h))
    {
        lusbd_ep_set_halt(dev_idx, 0, 0);
        LUSBD_INFO("Dev(%u) setup get_interface invalid, bmRequestType:0x%02x wValue:0x%02x%02x wLength:0x%02x%02x, ep0 halted",
                   dev_idx, setup_data->bmRequestType, setup_data->wValue_h, setup_data->wValue_l, setup_data->wLength_h, setup_data->wLength_l);
        return;
    }
    dev = &lusbd_dev_list[dev_idx];

    if (USB_STATE_CONFIGURED > dev->dev_state)
    {
        lusbd_ep_set_halt(dev_idx, 0, 0);
        LUSBD_INFO("Dev(%u) setup get_interface unsupported in unconfigured state(%u), ep0 halted", dev_idx, dev->dev_state);
        return;
    }
    if (LUSBD_IF_LIST_LEN <= setup_data->wIndex_l)
    {
        lusbd_ep_set_halt(dev_idx, 0, 0);
        LUSBD_INFO("Dev(%u) setup get_interface invalid, if_num(%u) exceeds the max(%u), ep0 halted", dev_idx, setup_data->wIndex_l, LUSBD_IF_LIST_LEN);
        return;
    }
    active_if = &dev->active_ifs[setup_data->wIndex_l];

    if (0 == active_if->used)
    {
        lusbd_ep_set_halt(dev_idx, 0, 0);
        LUSBD_INFO("Dev(%u) setup get_interface invalid, if_num:%u, ep0 halted", dev_idx, setup_data->wIndex_l);
        return;
    }

    dev->tx_buf.alt_setting = active_if->cur_alt;
    lusbd_ep_tx(dev_idx, 0, &dev->tx_buf.alt_setting, sizeof(dev->tx_buf.alt_setting), 0);
    lusbd_control_rx_status(dev_idx);
    LUSBD_DEBUG("Dev(%u) setup get_interface %u", dev_idx, active_if->cur_alt);
}

static void setup_set_interface(unsigned int dev_idx, const usb_setup_data_t *setup_data)
{
    lusbd_ep_set_halt(dev_idx, 0, 0);
    LUSBD_INFO("Dev(%u) setup set_interface unsupported yet, ep0 halted", dev_idx);
    return;
}

static void setup_synch_frame(unsigned int dev_idx, const usb_setup_data_t *setup_data)
{
    lusbd_ep_set_halt(dev_idx, 0, 0);
    LUSBD_INFO("Dev(%u) setup synch_frame unsupported yet, ep0 halted", dev_idx);
    return;

    if (   (0x82 != setup_data->bmRequestType)
        || (0 != setup_data->wValue_l)
        || (0 != setup_data->wValue_h)
        || (2 != setup_data->wLength_l)
        || (0 != setup_data->wLength_h))
    {
        lusbd_ep_set_halt(dev_idx, 0, 0);
        LUSBD_INFO("Dev(%u) setup synch_frame invalid, bmRequestType:0x%02x wValue:0x%02x%02x wLength:0x%02x%02x, ep0 halted",
                   dev_idx, setup_data->bmRequestType, setup_data->wValue_h, setup_data->wValue_l, setup_data->wLength_h, setup_data->wLength_l);
        return;
    }
}

void lusbd_setup_handler(unsigned int dev_idx, const uint8_t *setup_data)
{
    usb_setup_data_t *data = (usb_setup_data_t *)setup_data;
    lusbd_if_t *active_if;
    lusbd_class_t *cls;


    if ((LUSBD_DEV_COUNT <= dev_idx) || (NULL == setup_data))
        return;

    //LUSBD_DEBUG("Dev(%u) setup, bmRequestType:%02x  req:%u value:0x%02x%02x idx:0x%02x%02x len:0x%02x%02x",
    //            dev_idx, data->bmRequestType, data->bRequest,
    //            data->wValue_h, data->wValue_l,
    //            data->wIndex_h, data->wIndex_l,
    //            data->wLength_h, data->wLength_l);

    if (USB_REQ_TYPE_STANDARD == data->bmRequestType_type)
    {
        switch (data->bRequest)
        {
            case USB_REQ_CODE_GET_STATUS:
            {
                setup_get_status(dev_idx, data);
                break;
            }
            case USB_REQ_CODE_CLEAR_FEATURE:
            {
                setup_clear_feature(dev_idx, data);
                break;
            }
            case USB_REQ_CODE_SET_FEATURE:
            {
                setup_set_feature(dev_idx, data);
                break;
            }
            case USB_REQ_CODE_SET_ADDRESS:
            {
                setup_set_address(dev_idx, data);
                break;
            }
            case USB_REQ_CODE_GET_DESCRIPTOR:
            {
                setup_get_descriptor(dev_idx, data);
                break;
            }
            case USB_REQ_CODE_SET_DESCRIPTOR:
            {
                setup_set_descriptor(dev_idx, data);
                break;
            }
            case USB_REQ_CODE_GET_CONFIGURATION:
            {
                setup_get_config(dev_idx, data);
                break;
            }
            case USB_REQ_CODE_SET_CONFIGURATION:
            {
                setup_set_config(dev_idx, data);
                break;
            }
            case USB_REQ_CODE_GET_INTERFACE:
            {
                setup_get_interface(dev_idx, data);
                break;
            }
            case USB_REQ_CODE_SET_INTERFACE:
            {
                setup_set_interface(dev_idx, data);
                break;
            }
            case USB_REQ_CODE_SYNCH_FRAME:
            {
                setup_synch_frame(dev_idx, data);
                break;
            }
            default:
            {
                lusbd_ep_set_halt(dev_idx, 0, 0);
                LUSBD_INFO("Dev(%u) setup standard request(%u) unsupported, ep0 halted", dev_idx, data->bRequest);
                return;
            }
        }
    }
    else if ((USB_REQ_TYPE_CLASS == data->bmRequestType_type) || (USB_REQ_TYPE_VENDOR == data->bmRequestType_type))
    {
        if (USB_REQ_RECIPIENT_INTERFACE == data->bmRequestType_recipient)
        {
            if (USB_STATE_CONFIGURED > lusbd_dev_list[dev_idx].dev_state)
            {
                lusbd_ep_set_halt(dev_idx, 0, 0);
                LUSBD_INFO("Dev(%u) setup bmRequestType(0x%02x) unsupported in unconfigured state(%u), ep0 halted",
                            dev_idx, data->bmRequestType, lusbd_dev_list[dev_idx].dev_state);
                return;
            }
            if (LUSBD_IF_LIST_LEN <= data->wIndex_l)
            {
                lusbd_ep_set_halt(dev_idx, 0, 0);
                LUSBD_INFO("Dev(%u) setup invalid, if_num(%u) exceeds the max(%u), ep0 halted", dev_idx, data->wIndex_l, LUSBD_IF_LIST_LEN);
                return;
            }
            active_if = &lusbd_dev_list[dev_idx].active_ifs[data->wIndex_l];

            if (0 == active_if->used)
            {
                lusbd_ep_set_halt(dev_idx, 0, 0);
                LUSBD_INFO("Dev(%u) setup invalid, if_num:%u, ep0 halted", dev_idx, data->wIndex_l);
                return;
            }
            cls = &lusbd_class_list[active_if->class_idx];
            if (cls->cb && cls->cb->setup)
            {
                cls->cb->setup(dev_idx, data, cls->class_data);
            }
            else
            {
                lusbd_ep_set_halt(dev_idx, 0, 0);
                LUSBD_INFO("Dev(%u) setup if_num(%u) class cb is NULL, ep0 halted", dev_idx, data->wIndex_l);
                return;
            }
        }
        else if (USB_REQ_RECIPIENT_ENDPOINT == data->bmRequestType_recipient)
        {
            lusbd_ep_set_halt(dev_idx, 0, 0);
            LUSBD_INFO("Dev(%u) setup request_type(%u) recipient(EP) unsupported yet, ep0 halted", dev_idx, data->bmRequestType_type);
            return;
        }
        else
        {
            lusbd_ep_set_halt(dev_idx, 0, 0);
            LUSBD_INFO("Dev(%u) setup request_type(%u) recipient(%u) unsupported, ep0 halted",
                       dev_idx, data->bmRequestType_type, data->bmRequestType_recipient);
            return;
        }
    }
    else
    {
        lusbd_ep_set_halt(dev_idx, 0, 0);
        LUSBD_INFO("Dev(%u) setup invalid, request_type:%u, ep0 halted", dev_idx, data->bmRequestType_type);
        return;
    }
}

void lusbd_data_in_handler(unsigned int dev_idx, unsigned int ep_num)
{
    lusbd_dev_t *dev;
    lusbd_ep_t *ep;
    lusbd_class_t *cls;

    if (LUSBD_DEV_COUNT <= dev_idx)
        return;
    dev = &lusbd_dev_list[dev_idx];

    if (ep_num)
    {
        if (LUSBD_IN_EP_MAX_NUM < ep_num)
            return;
        ep = &dev->active_eps[1][ep_num];
        if (ep->need_zlp)
        {
            ep->need_zlp = 0;
            lusbd_hal_ep_tx(dev_idx, ep_num, NULL, 0);
        }
        else
        {
            cls = &lusbd_class_list[ep->class_idx];
            if (cls->cb && cls->cb->data_in_complete)
                cls->cb->data_in_complete(dev_idx, ep_num, cls->class_data);
        }
    }
    else
    {
        if (1 == dev->test_mode_state)
        {
            dev->test_mode_state = 2;
            lusbd_hal_set_test_mode(dev_idx, dev->test_mode);
        }
        if (NULL == dev->ep0_tx_buf)
            return;
        if ((LUSBD_MAX_PACKET_SIZE_EP0 * 2) <= dev->ep0_tx_len)
        {
            dev->ep0_tx_buf += LUSBD_MAX_PACKET_SIZE_EP0;
            dev->ep0_tx_len -= LUSBD_MAX_PACKET_SIZE_EP0;
            lusbd_hal_ep_tx(dev_idx, 0, dev->ep0_tx_buf, LUSBD_MAX_PACKET_SIZE_EP0);
            lusbd_control_rx_status(dev_idx);
        }
        else if (LUSBD_MAX_PACKET_SIZE_EP0 < dev->ep0_tx_len)
        {
            dev->ep0_tx_buf += LUSBD_MAX_PACKET_SIZE_EP0;
            dev->ep0_tx_len -= LUSBD_MAX_PACKET_SIZE_EP0;
            lusbd_hal_ep_tx(dev_idx, 0, dev->ep0_tx_buf, dev->ep0_tx_len);
            lusbd_control_rx_status(dev_idx);
            dev->ep0_tx_buf = NULL;
            dev->ep0_tx_len = 0;
            dev->need_zlp = 0;
        }
        else if (LUSBD_MAX_PACKET_SIZE_EP0 == dev->ep0_tx_len)
        {
            dev->ep0_tx_buf = NULL;
            dev->ep0_tx_len = 0;
            if (dev->need_zlp)
            {
                dev->need_zlp = 0;
                lusbd_hal_ep_tx(dev_idx, 0, NULL, 0);
            }
            lusbd_control_rx_status(dev_idx);
        }
    }
}

void lusbd_data_out_handler(unsigned int dev_idx, unsigned int ep_num, uint8_t *buf, unsigned int recv_len)
{
    lusbd_dev_t *dev;
    lusbd_ep_t *ep;
    lusbd_class_t *cls;

    if (LUSBD_DEV_COUNT <= dev_idx)
        return;
    dev = &lusbd_dev_list[dev_idx];

    if (ep_num)
    {
        if (LUSBD_OUT_EP_MAX_NUM < ep_num)
            return;
        ep = &dev->active_eps[0][ep_num];
        cls = &lusbd_class_list[ep->class_idx];
        if (cls->cb && cls->cb->data_out)
            cls->cb->data_out(dev_idx, ep_num, buf, recv_len, cls->class_data);
    }
    else
    {
        if (dev->is_rx_status)
        {
            dev->is_rx_status = 0;
            return;
        }
        if (dev->control_out_cb)
        {
            dev->control_out_cb(dev_idx, buf, recv_len, dev->control_out_arg, dev->control_out_class_data);
            dev->control_out_cb = NULL;
        }
        else
        {
            lusbd_ep_set_halt(dev_idx, 0, 0);
        }
    }
}
