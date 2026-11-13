#ifndef MINI_USB_PASS_ENROLL_H
#define MINI_USB_PASS_ENROLL_H

#include "common.h"
#include "protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LocalPassword {
  BOOL  used;
  WCHAR text[MAX_PASSWORD_LEN];
} LocalPassword;

typedef struct FingerState {
  BOOL enrolled;
  int  password_count;
  LocalPassword passwords[MAX_PASSWORDS_PER_FINGER];
} FingerState;

BOOL EnrollFingerprint(HANDLE device, int finger_index);
BOOL ClearFinger(HANDLE device, int finger_index);

BOOL AddPassword(HANDLE device, int finger_index, const WCHAR *password, FingerState *local);
BOOL DeletePassword(HANDLE device, int finger_index, int slot, FingerState *local);
BOOL RefreshPasswords(HANDLE device, int finger_index, FingerState *local);

BOOL InitDevice(HANDLE device);
BOOL SetLoginPassword(HANDLE device, const WCHAR *password);
BOOL BackupDevice(HANDLE device, const WCHAR *path);

/* Wide ↔ ASCII helpers shared by enroll/manage. */
BOOL WideToAsciiPassword(const WCHAR *wide, CHAR *out, int out_cap, UCHAR *out_len);

#ifdef __cplusplus
}
#endif

#endif /* MINI_USB_PASS_ENROLL_H */
