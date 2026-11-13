#include "device.h"

#include <initguid.h>
#include <setupapi.h>
#include <winioctl.h>
#include <usbiodef.h>

static const WCHAR *FindInsensitive(const WCHAR *hay, const WCHAR *needle)
{
  size_t nlen;
  if (!hay || !needle || !*needle) {
    return hay;
  }
  nlen = (size_t)lstrlenW(needle);
  for (; *hay; ++hay) {
    size_t i;
    for (i = 0; i < nlen; ++i) {
      WCHAR a = hay[i];
      WCHAR b = needle[i];
      if (a >= L'a' && a <= L'z') {
        a = (WCHAR)(a - L'a' + L'A');
      }
      if (b >= L'a' && b <= L'z') {
        b = (WCHAR)(b - L'a' + L'A');
      }
      if (!hay[i] || a != b) {
        break;
      }
    }
    if (i == nlen) {
      return hay;
    }
  }
  return NULL;
}

static BOOL ContainsVidPid(const WCHAR *text)
{
  WCHAR needle[64];
  if (!text) {
    return FALSE;
  }
  wsprintfW(needle, L"VID_%04X&PID_%04X", FP_VENDOR_ID, FP_PRODUCT_ID);
  return FindInsensitive(text, needle) != NULL;
}

static BOOL HardwareIdMatches(const WCHAR *hwids)
{
  const WCHAR *p;
  if (!hwids) {
    return FALSE;
  }
  for (p = hwids; *p; p += lstrlenW(p) + 1) {
    if (ContainsVidPid(p)) {
      return TRUE;
    }
  }
  return FALSE;
}

static BOOL DeviceInfoMatchesVidPid(HDEVINFO devs, SP_DEVINFO_DATA *dev_info)
{
  WCHAR *hwids = NULL;
  DWORD req = 0;
  BOOL ok = FALSE;

  SetupDiGetDeviceRegistryPropertyW(devs, dev_info, SPDRP_HARDWAREID,
                                    NULL, NULL, 0, &req);
  if (req == 0) {
    return FALSE;
  }

  hwids = (WCHAR *)LocalAlloc(LPTR, req);
  if (!hwids) {
    return FALSE;
  }

  if (SetupDiGetDeviceRegistryPropertyW(devs, dev_info, SPDRP_HARDWAREID,
                                        NULL, (PBYTE)hwids, req, NULL)) {
    ok = HardwareIdMatches(hwids);
  }

  LocalFree(hwids);
  return ok;
}

static HANDLE TryOpenInterface(HDEVINFO devs, const GUID *guid)
{
  DWORD index;

  for (index = 0;; ++index) {
    SP_DEVICE_INTERFACE_DATA if_data;
    PSP_DEVICE_INTERFACE_DETAIL_DATA_W detail = NULL;
    SP_DEVINFO_DATA dev_info;
    DWORD req = 0;
    HANDLE h;
    BOOL match;

    if_data.cbSize = sizeof(if_data);
    if (!SetupDiEnumDeviceInterfaces(devs, NULL, guid, index, &if_data)) {
      break;
    }

    SetupDiGetDeviceInterfaceDetailW(devs, &if_data, NULL, 0, &req, NULL);
    if (req == 0) {
      continue;
    }

    detail = (PSP_DEVICE_INTERFACE_DETAIL_DATA_W)LocalAlloc(LPTR, req);
    if (!detail) {
      continue;
    }
    detail->cbSize = sizeof(*detail);
    dev_info.cbSize = sizeof(dev_info);

    if (!SetupDiGetDeviceInterfaceDetailW(devs, &if_data, detail, req, NULL, &dev_info)) {
      LocalFree(detail);
      continue;
    }

    match = DeviceInfoMatchesVidPid(devs, &dev_info) ||
            ContainsVidPid(detail->DevicePath);
    if (!match) {
      LocalFree(detail);
      continue;
    }

    h = CreateFileW(detail->DevicePath,
                    GENERIC_READ | GENERIC_WRITE,
                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    LocalFree(detail);
    if (h != INVALID_HANDLE_VALUE) {
      return h;
    }
  }

  return INVALID_HANDLE_VALUE;
}

static HANDLE OpenByGuid(const GUID *guid)
{
  HDEVINFO devs;
  HANDLE h;

  devs = SetupDiGetClassDevsW(guid, NULL, NULL,
                              DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
  if (devs == INVALID_HANDLE_VALUE) {
    return INVALID_HANDLE_VALUE;
  }

  h = TryOpenInterface(devs, guid);
  SetupDiDestroyDeviceInfoList(devs);
  return h;
}

HANDLE DeviceOpen(void)
{
  HANDLE h;

  h = OpenByGuid(&GUID_DEVINTERFACE_DISK);
  if (h != INVALID_HANDLE_VALUE) {
    return h;
  }

  h = OpenByGuid(&GUID_DEVINTERFACE_USB_DEVICE);
  if (h != INVALID_HANDLE_VALUE) {
    return h;
  }

  SetLastError(ERROR_FILE_NOT_FOUND);
  return INVALID_HANDLE_VALUE;
}

void DeviceClose(HANDLE h)
{
  if (h != NULL && h != INVALID_HANDLE_VALUE) {
    CloseHandle(h);
  }
}

BOOL DeviceSendCommand(HANDLE h, DWORD ioctl, const void *in_buf, DWORD in_len,
                       void *out_buf, DWORD out_len, DWORD *bytes_returned)
{
  DWORD br = 0;
  BOOL ok;

  if (h == NULL || h == INVALID_HANDLE_VALUE) {
    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
  }

  ok = DeviceIoControl(h, ioctl,
                       (LPVOID)in_buf, in_len,
                       out_buf, out_len,
                       &br, NULL);
  if (bytes_returned) {
    *bytes_returned = br;
  }
  return ok;
}

BOOL DeviceProbe(HANDLE h, FpDeviceInfo *info)
{
  FpDeviceInfo local;
  DWORD br = 0;

  ZeroMemory(&local, sizeof(local));
  if (!DeviceSendCommand(h, IOCTL_FP_GET_INFO, NULL, 0, &local, sizeof(local), &br)) {
    return FALSE;
  }
  if (info) {
    *info = local;
  }
  return TRUE;
}
