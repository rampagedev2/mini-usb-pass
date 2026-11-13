#include "ui.h"
#include "common.h"
#include "device.h"
#include "enroll.h"
#include "protocol.h"

#include <commctrl.h>
#include <commdlg.h>

enum {
  ID_TABS = 2000,
  ID_BTN_CONNECT = 2001,
  ID_STATUS = 2002,
  ID_STRIP_FINGER = 2100,
  ID_STRIP_PASS = 2101,
  ID_BTN_ENROLL = 2301,
  ID_BTN_CLEAR_FINGER = 2302,
  ID_HINT_FINGER = 2303,
  ID_LIST_PASS = 2400,
  ID_EDIT_PASS = 2401,
  ID_EDIT_PASS2 = 2402,
  ID_BTN_ADD_PASS = 2403,
  ID_BTN_DEL_PASS = 2404,
  ID_HINT_PASS = 2405,
  ID_BTN_INIT = 2501,
  ID_EDIT_LOGIN = 2502,
  ID_EDIT_LOGIN2 = 2503,
  ID_BTN_SET_LOGIN = 2504,
  ID_BTN_BACKUP = 2505,
  ID_HINT_MANAGE = 2506
};

enum { TAB_FINGER = 0, TAB_PASSWORD = 1, TAB_MANAGE = 2 };

#define WM_STRIP_SELCHANGED (WM_USER + 40)

typedef struct AppState {
  HINSTANCE inst;
  HWND hwnd;
  HWND tabs;
  HWND panel_finger;
  HWND panel_pass;
  HWND panel_manage;
  HWND status;
  HWND btn_connect;
  HWND strip_finger;
  HWND strip_pass;
  HWND list_pass;
  HWND edit_pass;
  HWND edit_pass2;
  HWND edit_login;
  HWND edit_login2;
  HFONT font_ui;
  HFONT font_title;
  HFONT font_small;
  HFONT font_chip;
  HBRUSH br_bg;
  HBRUSH br_panel;
  HBRUSH br_header;
  HBRUSH br_footer;
  HBRUSH br_accent;
  HBRUSH br_chip;
  HANDLE device;
  int selected_finger;
  BOOL connected;
  FingerState fingers[MAX_FINGERS];
} AppState;

static AppState g;

/* ---- status helpers ---- */

static void SetStatus(const WCHAR *text)
{
  if (g.status) {
    SetWindowTextW(g.status, text);
  }
}

static void SetStatusError(const WCHAR *prefix)
{
  WCHAR buf[160];
  wsprintfW(buf, L"%s  ·  err %lu", prefix, GetLastError());
  SetStatus(buf);
}

static BOOL RequireDevice(void)
{
  if (!g.connected || g.device == INVALID_HANDLE_VALUE) {
    SetStatus(L"Connect the device first");
    return FALSE;
  }
  return TRUE;
}

static void UpdateConnectButton(void)
{
  SetWindowTextW(g.btn_connect, g.connected ? L"Disconnect" : L"Connect device");
  InvalidateRect(g.hwnd, NULL, FALSE);
}

static void ShowTab(int tab)
{
  ShowWindow(g.panel_finger, tab == TAB_FINGER ? SW_SHOW : SW_HIDE);
  ShowWindow(g.panel_pass, tab == TAB_PASSWORD ? SW_SHOW : SW_HIDE);
  ShowWindow(g.panel_manage, tab == TAB_MANAGE ? SW_SHOW : SW_HIDE);
}

/* ---- simple clear hands (flat GDI) ----
 * Left hand figure; right hand = mirror.
 * 0-4 left pinky→thumb, 5-9 right thumb→pinky
 */

#define HAND_FILL   RGB(241, 245, 249)
#define HAND_LINE   RGB(71, 85, 105)
#define HAND_SEL    COL_ACCENT
#define HAND_SEL_LN COL_ACCENT_DK

typedef struct FingerGeom {
  RECT rc; /* clickable / drawable bounds */
} FingerGeom;

typedef struct HandGeom {
  int cx;
  RECT palm;
  FingerGeom fingers[5]; /* 0 pinky .. 3 index, 4 thumb */
  BOOL mirrored;
} HandGeom;

static void FillRound(HDC hdc, const RECT *r, int rad, COLORREF fill, COLORREF line)
{
  HBRUSH br = CreateSolidBrush(fill);
  HPEN pen = CreatePen(PS_SOLID, 2, line);
  HGDIOBJ obr = SelectObject(hdc, br);
  HGDIOBJ open = SelectObject(hdc, pen);
  RoundRect(hdc, r->left, r->top, r->right, r->bottom, rad, rad);
  SelectObject(hdc, obr);
  SelectObject(hdc, open);
  DeleteObject(br);
  DeleteObject(pen);
}

#define HAND_PALM_H   60
#define HAND_MAX_LEN  78
#define HAND_TOP_PAD  10

static void LayoutLeftHand(HandGeom *hand, int cx, int base_y)
{
  /* pinky, ring, middle, index heights */
  static const int lens[4] = {48, 64, HAND_MAX_LEN, 68};
  static const int fw = 22;
  static const int gap = 5;
  int palm_w = 102;
  int span = 4 * fw + 3 * gap;
  int start = cx - span / 2;
  int i;

  hand->mirrored = FALSE;
  hand->cx = cx;
  SetRect(&hand->palm, cx - palm_w / 2, base_y, cx + palm_w / 2, base_y + HAND_PALM_H);

  for (i = 0; i < 4; ++i) {
    int x = start + i * (fw + gap);
    /* Sit on palm top — do not tuck under palm */
    SetRect(&hand->fingers[i].rc,
            x, base_y - lens[i],
            x + fw, base_y);
  }

  /* Thumb — right of palm, fully outside palm box */
  SetRect(&hand->fingers[4].rc,
          hand->palm.right + 2, base_y + 14,
          hand->palm.right + 36, base_y + 14 + 38);
}

static void MirrorHand(const HandGeom *src, HandGeom *dst, int dst_cx)
{
  int i;
  int src_cx = src->cx;

  dst->mirrored = TRUE;
  dst->cx = dst_cx;
  SetRect(&dst->palm,
          dst_cx - (src->palm.right - src_cx),
          src->palm.top,
          dst_cx - (src->palm.left - src_cx),
          src->palm.bottom);

  for (i = 0; i < 5; ++i) {
    const RECT *s = &src->fingers[i].rc;
    SetRect(&dst->fingers[i].rc,
            dst_cx - (s->right - src_cx),
            s->top,
            dst_cx - (s->left - src_cx),
            s->bottom);
  }
}

static void StripLayoutHands(HWND hwnd, HandGeom *left, HandGeom *right)
{
  RECT rc;
  int mid, base_y;
  GetClientRect(hwnd, &rc);
  mid = rc.right / 2;
  /* Keep full finger length visible: top pad + tallest finger */
  base_y = HAND_TOP_PAD + HAND_MAX_LEN;
  if (base_y + HAND_PALM_H + 8 > rc.bottom) {
    base_y = rc.bottom - HAND_PALM_H - 8;
  }
  LayoutLeftHand(left, mid / 2 - 10, base_y);
  MirrorHand(left, right, mid + mid / 2 + 10);
}

static void StripMapFingers(const HandGeom *left, const HandGeom *right, FingerGeom out[MAX_FINGERS])
{
  out[0] = left->fingers[0];
  out[1] = left->fingers[1];
  out[2] = left->fingers[2];
  out[3] = left->fingers[3];
  out[4] = left->fingers[4];
  out[5] = right->fingers[4];
  out[6] = right->fingers[3];
  out[7] = right->fingers[2];
  out[8] = right->fingers[1];
  out[9] = right->fingers[0];
}

static int StripHitTest(HWND hwnd, int x, int y)
{
  HandGeom left, right;
  FingerGeom all[MAX_FINGERS];
  POINT pt = {x, y};
  int i;
  StripLayoutHands(hwnd, &left, &right);
  StripMapFingers(&left, &right, all);
  for (i = 0; i < MAX_FINGERS; ++i) {
    RECT hit = all[i].rc;
    InflateRect(&hit, 3, 3);
    if (PtInRect(&hit, pt)) return i;
  }
  return -1;
}

static void PaintPalm(HDC hdc, const HandGeom *hand, const WCHAR *caption)
{
  RECT label = hand->palm;
  FillRound(hdc, &hand->palm, 18, HAND_FILL, HAND_LINE);
  label.top = hand->palm.bottom - 18;
  SetTextColor(hdc, HAND_LINE);
  SelectObject(hdc, g.font_small);
  DrawTextW(hdc, caption, -1, &label, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void PaintFingerGeom(HDC hdc, int index, const FingerGeom *f)
{
  WCHAR label[8];
  BOOL sel = (index == g.selected_finger);
  BOOL enrolled = g.fingers[index].enrolled;
  int count = g.fingers[index].password_count;
  COLORREF fill = sel ? HAND_SEL : HAND_FILL;
  COLORREF line = sel ? HAND_SEL_LN : HAND_LINE;
  COLORREF text = sel ? RGB(255, 255, 255) : HAND_LINE;
  int rad = (f->rc.right - f->rc.left);
  if (rad > f->rc.bottom - f->rc.top) {
    rad = f->rc.bottom - f->rc.top;
  }

  FillRound(hdc, &f->rc, rad, fill, line);

  if (enrolled) {
    int mx = (f->rc.left + f->rc.right) / 2;
    HBRUSH br = CreateSolidBrush(sel ? RGB(255, 255, 255) : COL_ACCENT);
    HPEN pen = (HPEN)GetStockObject(NULL_PEN);
    HGDIOBJ oldb = SelectObject(hdc, br);
    HGDIOBJ oldp = SelectObject(hdc, pen);
    Ellipse(hdc, mx - 3, f->rc.top + 5, mx + 3, f->rc.top + 11);
    SelectObject(hdc, oldb);
    SelectObject(hdc, oldp);
    DeleteObject(br);
  }

  {
    RECT lab = f->rc;
    wsprintfW(label, L"%d", index + 1);
    SetTextColor(hdc, text);
    SelectObject(hdc, g.font_chip);
    DrawTextW(hdc, label, -1, &lab, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
  }

  if (count > 0) {
    WCHAR b[4];
    RECT badge = f->rc;
    wsprintfW(b, L"%d", count);
    badge.top = f->rc.bottom - 14;
    SetTextColor(hdc, sel ? RGB(204, 251, 241) : COL_MUTED);
    SelectObject(hdc, g.font_small);
    DrawTextW(hdc, b, -1, &badge, DT_CENTER | DT_SINGLELINE);
  }
}

static void PaintFingerStrip(HWND hwnd, HDC hdc)
{
  RECT rc;
  HandGeom left, right;
  FingerGeom all[MAX_FINGERS];
  int i;

  GetClientRect(hwnd, &rc);
  FillRect(hdc, &rc, g.br_panel);
  SetBkMode(hdc, TRANSPARENT);

  StripLayoutHands(hwnd, &left, &right);
  StripMapFingers(&left, &right, all);

  /* Palm under fingers so nothing covers fingertips */
  PaintPalm(hdc, &left, L"Left");
  PaintPalm(hdc, &right, L"Right");

  for (i = 0; i < MAX_FINGERS; ++i) {
    if (i != g.selected_finger) PaintFingerGeom(hdc, i, &all[i]);
  }
  if (g.selected_finger >= 0 && g.selected_finger < MAX_FINGERS) {
    PaintFingerGeom(hdc, g.selected_finger, &all[g.selected_finger]);
  }
}

static LRESULT CALLBACK StripProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg) {
  case WM_ERASEBKGND:
    return 1;
  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    PaintFingerStrip(hwnd, hdc);
    EndPaint(hwnd, &ps);
    return 0;
  }
  case WM_LBUTTONDOWN: {
    int hit = StripHitTest(hwnd, LOWORD(lParam), HIWORD(lParam));
    if (hit >= 0 && hit != g.selected_finger) {
      g.selected_finger = hit;
      InvalidateRect(g.strip_finger, NULL, FALSE);
      InvalidateRect(g.strip_pass, NULL, FALSE);
      SendMessageW(GetParent(hwnd), WM_STRIP_SELCHANGED, (WPARAM)hit, 0);
    }
    return 0;
  }
  case WM_SETCURSOR:
    SetCursor(LoadCursor(NULL, IDC_HAND));
    return TRUE;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static HWND CreateFingerStrip(HWND parent, int id, int x, int y, int w, int h)
{
  return CreateWindowExW(0, APP_STRIP_CLASS, L"",
      WS_CHILD | WS_VISIBLE, x, y, w, h, parent, (HMENU)(INT_PTR)id, g.inst, NULL);
}

/* ---- password list ---- */

static void RefreshPasswordList(void)
{
  int i;
  FingerState *fs = &g.fingers[g.selected_finger];
  SendMessageW(g.list_pass, LB_RESETCONTENT, 0, 0);
  for (i = 0; i < MAX_PASSWORDS_PER_FINGER; ++i) {
    if (fs->passwords[i].used) {
      WCHAR line[MAX_PASSWORD_LEN + 16];
      wsprintfW(line, L"%d.  %s", i + 1, fs->passwords[i].text);
      SendMessageW(g.list_pass, LB_ADDSTRING, 0, (LPARAM)line);
    }
  }
}

static void SyncFingerFromDevice(void)
{
  if (g.connected) {
    RefreshPasswords(g.device, g.selected_finger, &g.fingers[g.selected_finger]);
  }
  RefreshPasswordList();
  InvalidateRect(g.strip_finger, NULL, FALSE);
  InvalidateRect(g.strip_pass, NULL, FALSE);
}

/* ---- actions ---- */

static void OnConnect(void)
{
  WCHAR ok_msg[96];
  FpDeviceInfo info;

  if (g.connected) {
    DeviceClose(g.device);
    g.device = INVALID_HANDLE_VALUE;
    g.connected = FALSE;
    UpdateConnectButton();
    SetStatus(L"Disconnected");
    return;
  }

  g.device = DeviceOpen();
  if (g.device == INVALID_HANDLE_VALUE) {
    g.connected = FALSE;
    UpdateConnectButton();
    SetStatusError(L"Device not found");
    return;
  }

  g.connected = TRUE;
  UpdateConnectButton();
  SyncFingerFromDevice();

  if (DeviceProbe(g.device, &info)) {
    wsprintfW(ok_msg, L"Connected  ·  VID_%04X&PID_%04X", info.vendor_id, info.product_id);
  } else {
    wsprintfW(ok_msg, L"Connected  ·  VID_%04X&PID_%04X", FP_VENDOR_ID, FP_PRODUCT_ID);
  }
  SetStatus(ok_msg);
}

static void OnEnroll(void)
{
  if (!RequireDevice()) {
    return;
  }
  if (!EnrollFingerprint(g.device, g.selected_finger)) {
    SetStatusError(L"Enroll failed");
    return;
  }
  g.fingers[g.selected_finger].enrolled = TRUE;
  InvalidateRect(g.strip_finger, NULL, FALSE);
  InvalidateRect(g.strip_pass, NULL, FALSE);
  {
    WCHAR msg[72];
    wsprintfW(msg, L"Finger %d enrolled", g.selected_finger + 1);
    SetStatus(msg);
  }
}

static void OnClearFinger(void)
{
  if (!RequireDevice()) {
    return;
  }
  if (!ClearFinger(g.device, g.selected_finger)) {
    SetStatusError(L"Clear failed");
    /* still clear local for UX when firmware rejects */
  }
  ZeroMemory(&g.fingers[g.selected_finger], sizeof(FingerState));
  RefreshPasswordList();
  InvalidateRect(g.strip_finger, NULL, FALSE);
  InvalidateRect(g.strip_pass, NULL, FALSE);
  {
    WCHAR msg[48];
    wsprintfW(msg, L"Finger %d cleared", g.selected_finger + 1);
    SetStatus(msg);
  }
}

static void OnAddPassword(void)
{
  WCHAR pass[MAX_PASSWORD_LEN];
  WCHAR pass2[MAX_PASSWORD_LEN];
  DWORD err;

  GetWindowTextW(g.edit_pass, pass, MAX_PASSWORD_LEN);
  GetWindowTextW(g.edit_pass2, pass2, MAX_PASSWORD_LEN);

  if (pass[0] == 0) {
    SetStatus(L"Enter a password");
    return;
  }
  if (lstrcmpW(pass, pass2) != 0) {
    SetStatus(L"Passwords do not match");
    return;
  }
  if (g.fingers[g.selected_finger].password_count >= MAX_PASSWORDS_PER_FINGER) {
    SetStatus(L"This finger already has 8 passwords");
    return;
  }

  SetLastError(0);
  if (!AddPassword(g.device, g.selected_finger, pass, &g.fingers[g.selected_finger])) {
    SetStatusError(L"Could not add password");
    return;
  }

  err = GetLastError();
  SetWindowTextW(g.edit_pass, L"");
  SetWindowTextW(g.edit_pass2, L"");
  RefreshPasswordList();
  InvalidateRect(g.strip_finger, NULL, FALSE);
  InvalidateRect(g.strip_pass, NULL, FALSE);

  if (err == ERROR_IO_DEVICE) {
    SetStatus(L"Saved in app · device sync pending");
  } else {
    WCHAR msg[72];
    wsprintfW(msg, L"Password added for finger %d", g.selected_finger + 1);
    SetStatus(msg);
  }
}

static void OnDeletePassword(void)
{
  int sel = (int)SendMessageW(g.list_pass, LB_GETCURSEL, 0, 0);
  if (sel == LB_ERR) {
    SetStatus(L"Select a password in the list");
    return;
  }
  if (!DeletePassword(g.device, g.selected_finger, sel, &g.fingers[g.selected_finger])) {
    SetStatusError(L"Delete failed");
    return;
  }
  RefreshPasswordList();
  InvalidateRect(g.strip_finger, NULL, FALSE);
  InvalidateRect(g.strip_pass, NULL, FALSE);
  SetStatus(L"Password removed");
}

static void OnInitDevice(void)
{
  int i;
  if (!RequireDevice()) {
    return;
  }
  if (MessageBoxW(g.hwnd,
        L"Initialize the device?\nThis clears fingerprints and passwords on the stick.",
        APP_NAME, MB_ICONWARNING | MB_OKCANCEL) != IDOK) {
    return;
  }
  if (!InitDevice(g.device)) {
    SetStatusError(L"Init failed");
    return;
  }
  for (i = 0; i < MAX_FINGERS; ++i) {
    ZeroMemory(&g.fingers[i], sizeof(FingerState));
  }
  RefreshPasswordList();
  InvalidateRect(g.strip_finger, NULL, FALSE);
  InvalidateRect(g.strip_pass, NULL, FALSE);
  SetStatus(L"Device initialized");
}

static void OnSetLogin(void)
{
  WCHAR a[MAX_LOGIN_LEN];
  WCHAR b[MAX_LOGIN_LEN];
  if (!RequireDevice()) {
    return;
  }
  GetWindowTextW(g.edit_login, a, MAX_LOGIN_LEN);
  GetWindowTextW(g.edit_login2, b, MAX_LOGIN_LEN);
  if (a[0] == 0) {
    SetStatus(L"Enter a device login password");
    return;
  }
  if (lstrcmpW(a, b) != 0) {
    SetStatus(L"Login passwords do not match");
    return;
  }
  if (!SetLoginPassword(g.device, a)) {
    SetStatusError(L"Set login failed");
    return;
  }
  SetWindowTextW(g.edit_login, L"");
  SetWindowTextW(g.edit_login2, L"");
  SetStatus(L"Device login password updated");
}

static void OnBackup(void)
{
  OPENFILENAMEW ofn;
  WCHAR path[MAX_PATH];

  if (!RequireDevice()) {
    return;
  }

  lstrcpyW(path, L"usb_pass_backup.bin");
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = g.hwnd;
  ofn.lpstrFilter = L"Backup (*.bin)\0*.bin\0All files (*.*)\0*.*\0";
  ofn.lpstrFile = path;
  ofn.nMaxFile = MAX_PATH;
  ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
  ofn.lpstrDefExt = L"bin";

  if (!GetSaveFileNameW(&ofn)) {
    return;
  }
  if (!BackupDevice(g.device, path)) {
    SetStatusError(L"Backup failed");
    return;
  }
  SetStatus(L"Backup saved");
}

/* ---- layout / panels ---- */

static void Layout(HWND hwnd)
{
  RECT rc;
  RECT tr;
  int content_h;

  GetClientRect(hwnd, &rc);
  content_h = rc.bottom - TAB_TOP - FOOTER_H - 8;
  if (content_h < 160) {
    content_h = 160;
  }

  if (g.btn_connect) {
    MoveWindow(g.btn_connect, rc.right - 148, 18, 132, 28, TRUE);
  }
  if (g.tabs) {
    MoveWindow(g.tabs, 16, TAB_TOP, rc.right - 32, content_h, TRUE);
  }

  SetRect(&tr, 16, TAB_TOP, rc.right - 16, TAB_TOP + content_h);
  TabCtrl_AdjustRect(g.tabs, FALSE, &tr);

  if (g.panel_finger) {
    MoveWindow(g.panel_finger, tr.left, tr.top, tr.right - tr.left, tr.bottom - tr.top, TRUE);
  }
  if (g.panel_pass) {
    MoveWindow(g.panel_pass, tr.left, tr.top, tr.right - tr.left, tr.bottom - tr.top, TRUE);
  }
  if (g.panel_manage) {
    MoveWindow(g.panel_manage, tr.left, tr.top, tr.right - tr.left, tr.bottom - tr.top, TRUE);
  }
  if (g.status) {
    MoveWindow(g.status, 16, rc.bottom - FOOTER_H + 8, rc.right - 32, 20, TRUE);
  }
}

static HWND MkStatic(HWND parent, const WCHAR *text, int x, int y, int w, int h, int id, HFONT font)
{
  HWND hwnd = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE,
                              x, y, w, h, parent, (HMENU)(INT_PTR)id, g.inst, NULL);
  SendMessageW(hwnd, WM_SETFONT, (WPARAM)font, TRUE);
  return hwnd;
}

static HWND MkButton(HWND parent, const WCHAR *text, int x, int y, int w, int h, int id, BOOL defbtn)
{
  HWND hwnd = CreateWindowExW(0, L"BUTTON", text,
      WS_CHILD | WS_VISIBLE | (defbtn ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON),
      x, y, w, h, parent, (HMENU)(INT_PTR)id, g.inst, NULL);
  SendMessageW(hwnd, WM_SETFONT, (WPARAM)g.font_ui, TRUE);
  return hwnd;
}

static HWND MkEdit(HWND parent, int x, int y, int w, int h, int id, int limit)
{
  HWND hwnd = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
      WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_PASSWORD,
      x, y, w, h, parent, (HMENU)(INT_PTR)id, g.inst, NULL);
  SendMessageW(hwnd, WM_SETFONT, (WPARAM)g.font_ui, TRUE);
  SendMessageW(hwnd, EM_SETLIMITTEXT, limit - 1, 0);
  return hwnd;
}

static void CreateFingerPanel(HWND parent)
{
  g.panel_finger = CreateWindowExW(0, APP_PANEL_CLASS, L"",
      WS_CHILD | WS_VISIBLE, 0, 0, 100, 100, parent, NULL, g.inst, NULL);

  MkStatic(g.panel_finger, L"Select a finger on the hands", 20, 10, 320, 20, 0, g.font_ui);
  g.strip_finger = CreateFingerStrip(g.panel_finger, ID_STRIP_FINGER, 8, 28, 460, 160);

  MkStatic(g.panel_finger,
      L"Click a finger, place it on the sensor, then enroll.\n"
      L"Dot = enrolled. Number at base = password count.",
      20, 196, 400, 40, ID_HINT_FINGER, g.font_small);

  MkButton(g.panel_finger, L"Enroll fingerprint", 20, 244, 160, 34, ID_BTN_ENROLL, TRUE);
  MkButton(g.panel_finger, L"Clear slot", 192, 244, 110, 34, ID_BTN_CLEAR_FINGER, FALSE);
}

static void CreatePasswordPanel(HWND parent)
{
  g.panel_pass = CreateWindowExW(0, APP_PANEL_CLASS, L"",
      WS_CHILD, 0, 0, 100, 100, parent, NULL, g.inst, NULL);

  MkStatic(g.panel_pass, L"Finger", 20, 8, 120, 18, 0, g.font_ui);
  g.strip_pass = CreateFingerStrip(g.panel_pass, ID_STRIP_PASS, 8, 24, 460, 160);

  MkStatic(g.panel_pass, L"Saved passwords", 20, 192, 180, 18, 0, g.font_ui);
  g.list_pass = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
      WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY,
      20, 212, 300, 72, g.panel_pass, (HMENU)(INT_PTR)ID_LIST_PASS, g.inst, NULL);
  SendMessageW(g.list_pass, WM_SETFONT, (WPARAM)g.font_ui, TRUE);

  MkButton(g.panel_pass, L"Remove", 332, 212, 90, 28, ID_BTN_DEL_PASS, FALSE);

  MkStatic(g.panel_pass, L"New password", 20, 294, 140, 18, 0, g.font_ui);
  g.edit_pass = MkEdit(g.panel_pass, 20, 314, 300, 26, ID_EDIT_PASS, MAX_PASSWORD_LEN);
  MkStatic(g.panel_pass, L"Confirm", 20, 346, 140, 18, 0, g.font_ui);
  g.edit_pass2 = MkEdit(g.panel_pass, 20, 366, 300, 26, ID_EDIT_PASS2, MAX_PASSWORD_LEN);

  MkButton(g.panel_pass, L"Add password", 20, 402, 130, 30, ID_BTN_ADD_PASS, TRUE);

  MkStatic(g.panel_pass,
      L"Up to 8 passwords per finger. Touch types as a keyboard.",
      160, 406, 260, 32, ID_HINT_PASS, g.font_small);
}

static void CreateManagePanel(HWND parent)
{
  g.panel_manage = CreateWindowExW(0, APP_PANEL_CLASS, L"",
      WS_CHILD, 0, 0, 100, 100, parent, NULL, g.inst, NULL);

  MkStatic(g.panel_manage, L"Device management", 20, 16, 240, 22, 0, g.font_ui);
  MkStatic(g.panel_manage,
      L"Initialize wipes enrollments. Login password gates settings access on the stick. Backup exports device data.",
      20, 42, 420, 40, ID_HINT_MANAGE, g.font_small);

  MkButton(g.panel_manage, L"Initialize device", 20, 96, 160, 34, ID_BTN_INIT, FALSE);

  MkStatic(g.panel_manage, L"Device login password", 20, 150, 220, 20, 0, g.font_ui);
  g.edit_login = MkEdit(g.panel_manage, 20, 174, 260, 26, ID_EDIT_LOGIN, MAX_LOGIN_LEN);
  MkStatic(g.panel_manage, L"Confirm login password", 20, 210, 220, 20, 0, g.font_ui);
  g.edit_login2 = MkEdit(g.panel_manage, 20, 234, 260, 26, ID_EDIT_LOGIN2, MAX_LOGIN_LEN);
  MkButton(g.panel_manage, L"Set login password", 20, 274, 160, 32, ID_BTN_SET_LOGIN, TRUE);

  MkButton(g.panel_manage, L"Backup device…", 20, 330, 160, 34, ID_BTN_BACKUP, FALSE);
}

/* ---- painting / window procs ---- */

static void PaintHeader(HDC hdc, RECT *rc)
{
  RECT hdr = {0, 0, rc->right, HEADER_H};
  RECT accent = {0, HEADER_H - 3, rc->right, HEADER_H};
  RECT title = {20, 12, rc->right - 160, 38};
  RECT sub = {20, 36, rc->right - 160, 56};
  RECT pill;
  HBRUSH br;
  HBRUSH old;
  HPEN pen;
  HPEN oldp;

  FillRect(hdc, &hdr, g.br_header);
  FillRect(hdc, &accent, g.br_accent);

  SetBkMode(hdc, TRANSPARENT);
  SetTextColor(hdc, RGB(255, 255, 255));
  SelectObject(hdc, g.font_title);
  DrawTextW(hdc, APP_NAME, -1, &title, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

  SetTextColor(hdc, RGB(148, 163, 184));
  SelectObject(hdc, g.font_small);
  DrawTextW(hdc, L"Fingerprint device settings", -1, &sub, DT_LEFT | DT_SINGLELINE);

  SetRect(&pill, rc->right - 166, 26, rc->right - 156, 36);
  br = CreateSolidBrush(g.connected ? COL_OK : COL_WARN);
  old = (HBRUSH)SelectObject(hdc, br);
  pen = CreatePen(PS_SOLID, 1, g.connected ? COL_OK : COL_WARN);
  oldp = (HPEN)SelectObject(hdc, pen);
  Ellipse(hdc, pill.left, pill.top, pill.right, pill.bottom);
  SelectObject(hdc, oldp);
  SelectObject(hdc, old);
  DeleteObject(pen);
  DeleteObject(br);
}

static void PaintFooter(HDC hdc, RECT *rc)
{
  RECT foot = {0, rc->bottom - FOOTER_H, rc->right, rc->bottom};
  RECT line = {0, rc->bottom - FOOTER_H, rc->right, rc->bottom - FOOTER_H + 1};
  FillRect(hdc, &foot, g.br_footer);
  FillRect(hdc, &line, (HBRUSH)GetStockObject(LTGRAY_BRUSH));
}

static LRESULT CALLBACK PanelProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg) {
  case WM_ERASEBKGND: {
    RECT rc;
    GetClientRect(hwnd, &rc);
    FillRect((HDC)wParam, &rc, g.br_panel);
    return 1;
  }
  case WM_CTLCOLORSTATIC: {
    HDC hdc = (HDC)wParam;
    int id = GetDlgCtrlID((HWND)lParam);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, (id == ID_HINT_FINGER || id == ID_HINT_PASS || id == ID_HINT_MANAGE)
                 ? COL_MUTED : COL_TEXT);
    return (LRESULT)g.br_panel;
  }
  case WM_CTLCOLORLISTBOX: {
    HDC hdc = (HDC)wParam;
    SetBkColor(hdc, RGB(255, 255, 255));
    SetTextColor(hdc, COL_TEXT);
    return (LRESULT)GetStockObject(WHITE_BRUSH);
  }
  case WM_STRIP_SELCHANGED:
    SyncFingerFromDevice();
    {
      WCHAR msg[48];
      wsprintfW(msg, L"Finger %d selected", g.selected_finger + 1);
      SetStatus(msg);
    }
    return 0;
  case WM_COMMAND:
    switch (LOWORD(wParam)) {
    case ID_BTN_ENROLL: OnEnroll(); break;
    case ID_BTN_CLEAR_FINGER: OnClearFinger(); break;
    case ID_BTN_ADD_PASS: OnAddPassword(); break;
    case ID_BTN_DEL_PASS: OnDeletePassword(); break;
    case ID_BTN_INIT: OnInitDevice(); break;
    case ID_BTN_SET_LOGIN: OnSetLogin(); break;
    case ID_BTN_BACKUP: OnBackup(); break;
    }
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg) {
  case WM_CREATE: {
    TCITEMW item;
    WCHAR ready[96];

    g.hwnd = hwnd;
    g.device = INVALID_HANDLE_VALUE;
    g.selected_finger = 0;

    g.btn_connect = MkButton(hwnd, L"Connect device", 0, 0, 132, 28, ID_BTN_CONNECT, FALSE);

    g.tabs = CreateWindowExW(0, WC_TABCONTROLW, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 0, 100, 100, hwnd, (HMENU)(INT_PTR)ID_TABS, g.inst, NULL);
    SendMessageW(g.tabs, WM_SETFONT, (WPARAM)g.font_ui, TRUE);

    ZeroMemory(&item, sizeof(item));
    item.mask = TCIF_TEXT;
    item.pszText = L"Finger";
    TabCtrl_InsertItem(g.tabs, 0, &item);
    item.pszText = L"Password";
    TabCtrl_InsertItem(g.tabs, 1, &item);
    item.pszText = L"Manage";
    TabCtrl_InsertItem(g.tabs, 2, &item);

    CreateFingerPanel(hwnd);
    CreatePasswordPanel(hwnd);
    CreateManagePanel(hwnd);

    wsprintfW(ready, L"Ready  ·  VID_%04X&PID_%04X", FP_VENDOR_ID, FP_PRODUCT_ID);
    g.status = CreateWindowExW(0, L"STATIC", ready,
        WS_CHILD | WS_VISIBLE, 0, 0, 100, 20, hwnd,
        (HMENU)(INT_PTR)ID_STATUS, g.inst, NULL);
    SendMessageW(g.status, WM_SETFONT, (WPARAM)g.font_small, TRUE);

    ShowTab(TAB_FINGER);
    Layout(hwnd);
    return 0;
  }
  case WM_SIZE:
    Layout(hwnd);
    return 0;
  case WM_ERASEBKGND:
    return 1;
  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc;
    GetClientRect(hwnd, &rc);
    FillRect(hdc, &rc, g.br_bg);
    PaintHeader(hdc, &rc);
    PaintFooter(hdc, &rc);
    EndPaint(hwnd, &ps);
    return 0;
  }
  case WM_CTLCOLORSTATIC: {
    HDC hdc = (HDC)wParam;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, COL_MUTED);
    return (LRESULT)g.br_footer;
  }
  case WM_NOTIFY: {
    NMHDR *hdr = (NMHDR *)lParam;
    if (hdr->idFrom == ID_TABS && hdr->code == TCN_SELCHANGE) {
      int tab = (int)TabCtrl_GetCurSel(g.tabs);
      ShowTab(tab);
      if (tab == TAB_PASSWORD) {
        RefreshPasswordList();
      }
    }
    return 0;
  }
  case WM_COMMAND:
    if (LOWORD(wParam) == ID_BTN_CONNECT) {
      OnConnect();
    }
    return 0;
  case WM_GETMINMAXINFO: {
    MINMAXINFO *mmi = (MINMAXINFO *)lParam;
    mmi->ptMinTrackSize.x = 500;
    mmi->ptMinTrackSize.y = 600;
    return 0;
  }
  case WM_DESTROY:
    DeviceClose(g.device);
    g.device = INVALID_HANDLE_VALUE;
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

BOOL UiInit(HINSTANCE inst)
{
  WNDCLASSEXW wc;
  INITCOMMONCONTROLSEX icc;

  g.inst = inst;
  ZeroMemory(g.fingers, sizeof(g.fingers));

  icc.dwSize = sizeof(icc);
  icc.dwICC = ICC_STANDARD_CLASSES | ICC_TAB_CLASSES;
  InitCommonControlsEx(&icc);

  g.font_ui = CreateFontW(15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
      DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
  g.font_title = CreateFontW(20, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET,
      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
      DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
  g.font_small = CreateFontW(12, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
      DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
  g.font_chip = CreateFontW(16, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET,
      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
      DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

  g.br_bg = CreateSolidBrush(COL_BG);
  g.br_panel = CreateSolidBrush(COL_PANEL);
  g.br_header = CreateSolidBrush(COL_HEADER);
  g.br_footer = CreateSolidBrush(COL_FOOTER);
  g.br_accent = CreateSolidBrush(COL_ACCENT);
  g.br_chip = CreateSolidBrush(COL_CHIP);

  ZeroMemory(&wc, sizeof(wc));
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = WndProc;
  wc.hInstance = inst;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = g.br_bg;
  wc.lpszClassName = APP_CLASS;
  wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
  wc.hIconSm = wc.hIcon;
  if (!RegisterClassExW(&wc)) {
    return FALSE;
  }

  ZeroMemory(&wc, sizeof(wc));
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = PanelProc;
  wc.hInstance = inst;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = g.br_panel;
  wc.lpszClassName = APP_PANEL_CLASS;
  if (!RegisterClassExW(&wc)) {
    return FALSE;
  }

  ZeroMemory(&wc, sizeof(wc));
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = StripProc;
  wc.hInstance = inst;
  wc.hCursor = LoadCursor(NULL, IDC_HAND);
  wc.hbrBackground = g.br_panel;
  wc.lpszClassName = APP_STRIP_CLASS;
  if (!RegisterClassExW(&wc)) {
    return FALSE;
  }

  return TRUE;
}

void UiShutdown(void)
{
  if (g.font_ui) DeleteObject(g.font_ui);
  if (g.font_title) DeleteObject(g.font_title);
  if (g.font_small) DeleteObject(g.font_small);
  if (g.font_chip) DeleteObject(g.font_chip);
  if (g.br_bg) DeleteObject(g.br_bg);
  if (g.br_panel) DeleteObject(g.br_panel);
  if (g.br_header) DeleteObject(g.br_header);
  if (g.br_footer) DeleteObject(g.br_footer);
  if (g.br_accent) DeleteObject(g.br_accent);
  if (g.br_chip) DeleteObject(g.br_chip);
}

HWND UiCreateMainWindow(HINSTANCE inst, int show)
{
  HWND hwnd = CreateWindowExW(0, APP_CLASS, APP_NAME,
      WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
      CW_USEDEFAULT, CW_USEDEFAULT, WIN_W, WIN_H,
      NULL, NULL, inst, NULL);
  if (!hwnd) {
    return NULL;
  }
  ShowWindow(hwnd, show);
  UpdateWindow(hwnd);
  return hwnd;
}

int UiMessageLoop(void)
{
  MSG msg;
  while (GetMessageW(&msg, NULL, 0, 0) > 0) {
    if (!IsDialogMessageW(g.hwnd, &msg)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }
  return (int)msg.wParam;
}
