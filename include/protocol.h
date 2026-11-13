#ifndef MINI_USB_PASS_PROTOCOL_H
#define MINI_USB_PASS_PROTOCOL_H

#include <windows.h>
#include <winioctl.h>
#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FP_VENDOR_ID   0x1209
#define FP_PRODUCT_ID  0x0001
#define FP_IOCTL_BASE  0x800

#define IOCTL_FP_GET_INFO \
  CTL_CODE(FILE_DEVICE_UNKNOWN, FP_IOCTL_BASE + 0, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_FP_ENROLL_FINGER \
  CTL_CODE(FILE_DEVICE_UNKNOWN, FP_IOCTL_BASE + 1, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_FP_ADD_PASSWORD \
  CTL_CODE(FILE_DEVICE_UNKNOWN, FP_IOCTL_BASE + 2, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_FP_CLEAR_FINGER \
  CTL_CODE(FILE_DEVICE_UNKNOWN, FP_IOCTL_BASE + 3, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_FP_LIST_PASSWORDS \
  CTL_CODE(FILE_DEVICE_UNKNOWN, FP_IOCTL_BASE + 4, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_FP_DELETE_PASSWORD \
  CTL_CODE(FILE_DEVICE_UNKNOWN, FP_IOCTL_BASE + 5, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_FP_INIT_DEVICE \
  CTL_CODE(FILE_DEVICE_UNKNOWN, FP_IOCTL_BASE + 6, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_FP_SET_LOGIN \
  CTL_CODE(FILE_DEVICE_UNKNOWN, FP_IOCTL_BASE + 7, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_FP_BACKUP \
  CTL_CODE(FILE_DEVICE_UNKNOWN, FP_IOCTL_BASE + 8, METHOD_BUFFERED, FILE_ANY_ACCESS)

#pragma pack(push, 1)

typedef struct FpDeviceInfo {
  USHORT vendor_id;
  USHORT product_id;
  UCHAR  max_fingers;
  UCHAR  max_passwords_per_finger;
  UCHAR  reserved[2];
} FpDeviceInfo;

typedef struct FpEnrollFingerRequest {
  UCHAR finger_index;
  UCHAR reserved[3];
} FpEnrollFingerRequest;

typedef struct FpPasswordEntry {
  UCHAR slot;          /* 0 .. MAX_PASSWORDS_PER_FINGER-1 */
  UCHAR password_len;
  UCHAR reserved[2];
  CHAR  password[MAX_PASSWORD_LEN];
} FpPasswordEntry;

typedef struct FpAddPasswordRequest {
  UCHAR finger_index;
  UCHAR password_len;
  UCHAR reserved[2];
  CHAR  password[MAX_PASSWORD_LEN];
} FpAddPasswordRequest;

typedef struct FpListPasswordsRequest {
  UCHAR finger_index;
  UCHAR reserved[3];
} FpListPasswordsRequest;

typedef struct FpListPasswordsResponse {
  UCHAR finger_index;
  UCHAR count;
  UCHAR reserved[2];
  FpPasswordEntry entries[MAX_PASSWORDS_PER_FINGER];
} FpListPasswordsResponse;

typedef struct FpDeletePasswordRequest {
  UCHAR finger_index;
  UCHAR slot;
  UCHAR reserved[2];
} FpDeletePasswordRequest;

typedef struct FpClearFingerRequest {
  UCHAR finger_index;
  UCHAR reserved[3];
} FpClearFingerRequest;

typedef struct FpSetLoginRequest {
  UCHAR password_len;
  UCHAR reserved[3];
  CHAR  password[MAX_LOGIN_LEN];
} FpSetLoginRequest;

typedef struct FpBackupResponse {
  DWORD size;
  BYTE  data[4096];
} FpBackupResponse;

#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif /* MINI_USB_PASS_PROTOCOL_H */
