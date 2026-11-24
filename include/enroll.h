#ifndef MINI_USB_PASS_ENROLL_H
#define MINI_USB_PASS_ENROLL_H

#include "common.h"
#include "protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Packed ASCII cache — keeps .data small (passwords are ASCII for HID). */
#pragma pack(push, 1)
typedef struct LocalPassword {
  UCHAR used;
  CHAR  text[MAX_PASSWORD_LEN];
} LocalPassword;

typedef struct FingerState {
  UCHAR enrolled;
  UCHAR password_count;
  LocalPassword passwords[MAX_PASSWORDS_PER_FINGER];
} FingerState;
#pragma pack(pop)

BOOL EnrollFingerprint(HANDLE device, int finger_index);
BOOL ClearFinger(HANDLE device, int finger_index);

BOOL AddPassword(HANDLE device, int finger_index, const WCHAR *password, FingerState *local);
BOOL DeletePassword(HANDLE device, int finger_index, int slot, FingerState *local);
BOOL RefreshPasswords(HANDLE device, int finger_index, FingerState *local);

BOOL InitDevice(HANDLE device);
BOOL SetLoginPassword(HANDLE device, const WCHAR *password);
BOOL BackupDevice(HANDLE device, const WCHAR *path);

BOOL WideToAsciiPassword(const WCHAR *wide, CHAR *out, int out_cap, UCHAR *out_len);
void AsciiPasswordToWide(const CHAR *in, WCHAR *out, int out_cap);

#ifdef __cplusplus
}
#endif

#endif /* MINI_USB_PASS_ENROLL_H */
