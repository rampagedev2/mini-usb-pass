#ifndef MINI_USB_PASS_COMMON_H
#define MINI_USB_PASS_COMMON_H

#include <windows.h>

#define APP_NAME           L"Mini USB Pass"
#define APP_CLASS          L"MiniUsbPassMain"
#define APP_PANEL_CLASS    L"MiniUsbPassPanel"
#define APP_STRIP_CLASS    L"MiniUsbPassFingerStrip"
#define MAX_FINGERS        10
#define MAX_PASSWORDS_PER_FINGER 8
#define MAX_PASSWORD_LEN   64
#define MAX_LOGIN_LEN      32

/* Slate + teal */
#define COL_BG        RGB(244, 247, 249)
#define COL_HEADER    RGB(27, 42, 50)
#define COL_ACCENT    RGB(13, 148, 136)
#define COL_ACCENT_DK RGB(15, 118, 110)
#define COL_PANEL     RGB(255, 255, 255)
#define COL_CHIP      RGB(241, 245, 249)
#define COL_BORDER    RGB(203, 213, 225)
#define COL_TEXT      RGB(30, 41, 59)
#define COL_MUTED     RGB(100, 116, 139)
#define COL_OK        RGB(22, 163, 74)
#define COL_WARN      RGB(220, 38, 38)
#define COL_FOOTER    RGB(241, 245, 249)

#define WIN_W  520
#define WIN_H  660
#define HEADER_H 64
#define FOOTER_H 36
#define TAB_TOP  78

#endif /* MINI_USB_PASS_COMMON_H */
