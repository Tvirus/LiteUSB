#ifndef _LITEUSB_DEVICE_CFG_H_
#define _LITEUSB_DEVICE_CFG_H_




#define LUSBD_LOGLEVEL_ERROR  1
#define LUSBD_LOGLEVEL_INFO   2
#define LUSBD_LOGLEVEL_DEBUG  3
#ifndef LUSBD_LOGLEVEL
#define LUSBD_LOGLEVEL  LUSBD_LOGLEVEL_ERROR
#endif
#define LUSBD_ERROR(fmt, arg...)
#define LUSBD_INFO(fmt, arg...)
#define LUSBD_DEBUG(fmt, arg...)
/**
 * #define LUSBD_ERROR(fmt, arg...)  do{if((LUSBD_LOGLEVEL_ERROR) <= (LUSBD_LOGLEVEL))printf("--LUSBD-- " fmt "\n", ##arg);}while(0)
 * #define LUSBD_INFO(fmt, arg...)   do{if((LUSBD_LOGLEVEL_INFO)  <= (LUSBD_LOGLEVEL))printf("--LUSBD-- " fmt "\n", ##arg);}while(0)
 * #define LUSBD_DEBUG(fmt, arg...)  do{if((LUSBD_LOGLEVEL_DEBUG) <= (LUSBD_LOGLEVEL))printf("--LUSBD-- " fmt "\n", ##arg);}while(0)
 */
#define LUSBD_PRINT(fmt, arg...)
/**
 * #define LUSBD_PRINT(fmt, arg...)  do{printf(fmt, ##arg);}while(0)
 */


#ifndef LUSBD_STR_MAX_COUNT
#define LUSBD_STR_MAX_COUNT  16
#endif

#ifndef LUSBD_CFG_DESC_LEN
#define LUSBD_CFG_DESC_LEN  128
#endif

#ifndef LUSBD_CFG_LIST_LEN
#define LUSBD_CFG_LIST_LEN  2
#endif

#ifndef LUSBD_CLASS_LIST_LEN
#define LUSBD_CLASS_LIST_LEN  2
#endif

#ifndef LUSBD_IF_LIST_LEN
#define LUSBD_IF_LIST_LEN  4
#endif

#ifndef LUSBD_EP_LIST_LEN
#define LUSBD_EP_LIST_LEN  8
#endif


#endif
