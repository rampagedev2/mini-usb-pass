#include "ui.h"

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR cmd, int show)
{
  int code;

  (void)prev;
  (void)cmd;

  if (!UiInit(inst)) {
    return 1;
  }
  if (!UiCreateMainWindow(inst, show)) {
    UiShutdown();
    return 1;
  }

  code = UiMessageLoop();
  UiShutdown();
  return code;
}
