#ifndef MINI_USB_PASS_DEVICE_H
#define MINI_USB_PASS_DEVICE_H

#include <windows.h>
#include "protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Open mass-storage/control handle for FP_VENDOR_ID / FP_PRODUCT_ID. */
HANDLE DeviceOpen(void);

void DeviceClose(HANDLE h);

BOOL DeviceSendCommand(HANDLE h, DWORD ioctl, const void *in_buf, DWORD in_len,
                       void *out_buf, DWORD out_len, DWORD *bytes_returned);

/* Optional: confirm device answers GET_INFO. */
BOOL DeviceProbe(HANDLE h, FpDeviceInfo *info);

#ifdef __cplusplus
}
#endif

#endif /* MINI_USB_PASS_DEVICE_H */
