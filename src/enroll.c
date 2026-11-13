#include "enroll.h"
#include "device.h"

BOOL WideToAsciiPassword(const WCHAR *wide, CHAR *out, int out_cap, UCHAR *out_len)
{
  int i;
  if (!wide || !out || out_cap <= 0) {
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  for (i = 0; wide[i] != 0 && i < out_cap; ++i) {
    WCHAR ch = wide[i];
    if (ch < 0x20 || ch > 0x7E) {
      SetLastError(ERROR_INVALID_PARAMETER);
      return FALSE;
    }
    out[i] = (CHAR)ch;
  }
  if (i == 0) {
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  if (i < out_cap) {
    out[i] = 0;
  }
  if (out_len) {
    *out_len = (UCHAR)i;
  }
  return TRUE;
}

static void AsciiToWide(const CHAR *in, UCHAR len, WCHAR *out, int out_cap)
{
  int i;
  int n = len;
  if (n >= out_cap) {
    n = out_cap - 1;
  }
  for (i = 0; i < n; ++i) {
    out[i] = (WCHAR)(unsigned char)in[i];
  }
  out[i] = 0;
}

static BOOL LocalAddPassword(FingerState *local, const WCHAR *password)
{
  int i;
  if (!local || !password) {
    return FALSE;
  }
  if (local->password_count >= MAX_PASSWORDS_PER_FINGER) {
    SetLastError(ERROR_INSUFFICIENT_BUFFER);
    return FALSE;
  }
  for (i = 0; i < MAX_PASSWORDS_PER_FINGER; ++i) {
    if (!local->passwords[i].used) {
      lstrcpynW(local->passwords[i].text, password, MAX_PASSWORD_LEN);
      local->passwords[i].used = TRUE;
      local->password_count++;
      return TRUE;
    }
  }
  SetLastError(ERROR_INSUFFICIENT_BUFFER);
  return FALSE;
}

static void LocalDeleteSlot(FingerState *local, int slot)
{
  int i;
  if (!local || slot < 0 || slot >= MAX_PASSWORDS_PER_FINGER) {
    return;
  }
  if (!local->passwords[slot].used) {
    return;
  }
  for (i = slot; i < MAX_PASSWORDS_PER_FINGER - 1; ++i) {
    local->passwords[i] = local->passwords[i + 1];
  }
  local->passwords[MAX_PASSWORDS_PER_FINGER - 1].used = FALSE;
  local->passwords[MAX_PASSWORDS_PER_FINGER - 1].text[0] = 0;
  if (local->password_count > 0) {
    local->password_count--;
  }
}

BOOL EnrollFingerprint(HANDLE device, int finger_index)
{
  FpEnrollFingerRequest req;
  if (device == NULL || device == INVALID_HANDLE_VALUE) {
    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
  }
  if (finger_index < 0 || finger_index >= MAX_FINGERS) {
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  ZeroMemory(&req, sizeof(req));
  req.finger_index = (UCHAR)finger_index;
  return DeviceSendCommand(device, IOCTL_FP_ENROLL_FINGER, &req, sizeof(req), NULL, 0, NULL);
}

BOOL ClearFinger(HANDLE device, int finger_index)
{
  FpClearFingerRequest req;
  if (device == NULL || device == INVALID_HANDLE_VALUE) {
    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
  }
  if (finger_index < 0 || finger_index >= MAX_FINGERS) {
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  ZeroMemory(&req, sizeof(req));
  req.finger_index = (UCHAR)finger_index;
  return DeviceSendCommand(device, IOCTL_FP_CLEAR_FINGER, &req, sizeof(req), NULL, 0, NULL);
}

BOOL AddPassword(HANDLE device, int finger_index, const WCHAR *password, FingerState *local)
{
  FpAddPasswordRequest req;
  UCHAR len = 0;
  BOOL device_ok;

  if (finger_index < 0 || finger_index >= MAX_FINGERS || password == NULL || !local) {
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }

  ZeroMemory(&req, sizeof(req));
  req.finger_index = (UCHAR)finger_index;
  if (!WideToAsciiPassword(password, req.password, (int)sizeof(req.password), &len)) {
    return FALSE;
  }
  req.password_len = len;

  device_ok = FALSE;
  if (device != NULL && device != INVALID_HANDLE_VALUE) {
    device_ok = DeviceSendCommand(device, IOCTL_FP_ADD_PASSWORD, &req, sizeof(req), NULL, 0, NULL);
  }

  if (!LocalAddPassword(local, password)) {
    return FALSE;
  }

  if (!device_ok) {
    SetLastError(ERROR_IO_DEVICE);
    /* Password kept in app list for review; device sync pending. */
  }
  return TRUE;
}

BOOL DeletePassword(HANDLE device, int finger_index, int slot, FingerState *local)
{
  FpDeletePasswordRequest req;

  if (finger_index < 0 || finger_index >= MAX_FINGERS ||
      slot < 0 || slot >= MAX_PASSWORDS_PER_FINGER || !local) {
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }

  ZeroMemory(&req, sizeof(req));
  req.finger_index = (UCHAR)finger_index;
  req.slot = (UCHAR)slot;

  if (device != NULL && device != INVALID_HANDLE_VALUE) {
    DeviceSendCommand(device, IOCTL_FP_DELETE_PASSWORD, &req, sizeof(req), NULL, 0, NULL);
  }

  LocalDeleteSlot(local, slot);
  return TRUE;
}

BOOL RefreshPasswords(HANDLE device, int finger_index, FingerState *local)
{
  FpListPasswordsRequest req;
  FpListPasswordsResponse resp;
  DWORD br = 0;
  int i;

  if (!local || finger_index < 0 || finger_index >= MAX_FINGERS) {
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }

  if (device == NULL || device == INVALID_HANDLE_VALUE) {
    return TRUE;
  }

  ZeroMemory(&req, sizeof(req));
  ZeroMemory(&resp, sizeof(resp));
  req.finger_index = (UCHAR)finger_index;

  if (!DeviceSendCommand(device, IOCTL_FP_LIST_PASSWORDS,
                         &req, sizeof(req), &resp, sizeof(resp), &br)) {
    return FALSE;
  }

  ZeroMemory(local->passwords, sizeof(local->passwords));
  local->password_count = 0;
  if (resp.count > MAX_PASSWORDS_PER_FINGER) {
    resp.count = MAX_PASSWORDS_PER_FINGER;
  }
  for (i = 0; i < resp.count; ++i) {
    AsciiToWide(resp.entries[i].password, resp.entries[i].password_len,
                local->passwords[i].text, MAX_PASSWORD_LEN);
    local->passwords[i].used = TRUE;
    local->password_count++;
  }
  return TRUE;
}

BOOL InitDevice(HANDLE device)
{
  if (device == NULL || device == INVALID_HANDLE_VALUE) {
    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
  }
  return DeviceSendCommand(device, IOCTL_FP_INIT_DEVICE, NULL, 0, NULL, 0, NULL);
}

BOOL SetLoginPassword(HANDLE device, const WCHAR *password)
{
  FpSetLoginRequest req;
  UCHAR len = 0;

  if (device == NULL || device == INVALID_HANDLE_VALUE) {
    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
  }
  ZeroMemory(&req, sizeof(req));
  if (!WideToAsciiPassword(password, req.password, (int)sizeof(req.password), &len)) {
    return FALSE;
  }
  req.password_len = len;
  return DeviceSendCommand(device, IOCTL_FP_SET_LOGIN, &req, sizeof(req), NULL, 0, NULL);
}

BOOL BackupDevice(HANDLE device, const WCHAR *path)
{
  FpBackupResponse resp;
  DWORD br = 0;
  HANDLE file;
  DWORD written = 0;

  if (device == NULL || device == INVALID_HANDLE_VALUE || path == NULL) {
    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
  }

  ZeroMemory(&resp, sizeof(resp));
  if (!DeviceSendCommand(device, IOCTL_FP_BACKUP, NULL, 0, &resp, sizeof(resp), &br)) {
    return FALSE;
  }
  if (resp.size == 0 || resp.size > sizeof(resp.data)) {
    SetLastError(ERROR_INVALID_DATA);
    return FALSE;
  }

  file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (file == INVALID_HANDLE_VALUE) {
    return FALSE;
  }
  if (!WriteFile(file, resp.data, resp.size, &written, NULL) || written != resp.size) {
    CloseHandle(file);
    return FALSE;
  }
  CloseHandle(file);
  return TRUE;
}
