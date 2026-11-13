#ifndef MINI_USB_PASS_UI_H
#define MINI_USB_PASS_UI_H

#include <windows.h>

BOOL UiInit(HINSTANCE inst);
void UiShutdown(void);
HWND UiCreateMainWindow(HINSTANCE inst, int show);
int  UiMessageLoop(void);

#endif /* MINI_USB_PASS_UI_H */
