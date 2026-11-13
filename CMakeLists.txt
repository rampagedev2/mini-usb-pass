cmake_minimum_required(VERSION 3.16)
project(mini_usb_pass C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Hard size budget for embedding on device flash
set(MAX_EXE_BYTES 102400) # 100 KB

option(MINI_USB_PASS_SIZE_CHECK "Fail build if exe exceeds 100 KB" ON)

add_executable(mini_usb_pass WIN32
  src/main.c
  src/ui.c
  src/device.c
  src/enroll.c
  src/app.rc
)

target_include_directories(mini_usb_pass PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(mini_usb_pass PRIVATE
  WIN32_LEAN_AND_MEAN
  NOMINMAX
  UNICODE
  _UNICODE
)

# Prefer size over speed; keep CRT usage minimal via system DLLs only.
if(MSVC)
  target_compile_options(mini_usb_pass PRIVATE /O1 /GS- /Gy /W4)
  target_link_options(mini_usb_pass PRIVATE /OPT:REF /OPT:ICF /INCREMENTAL:NO)
else()
  target_compile_options(mini_usb_pass PRIVATE
    -Os
    -ffunction-sections
    -fdata-sections
    -fno-asynchronous-unwind-tables
    -fno-exceptions
    -Wall
    -Wextra
  )
  target_link_options(mini_usb_pass PRIVATE
    -municode
    -s
    -Wl,--gc-sections
    -Wl,--as-needed
    -static-libgcc
  )
endif()

target_link_libraries(mini_usb_pass PRIVATE
  user32
  gdi32
  comctl32
  comdlg32
  setupapi
  advapi32
)

if(MINI_USB_PASS_SIZE_CHECK)
  add_custom_command(TARGET mini_usb_pass POST_BUILD
    COMMAND ${CMAKE_COMMAND}
      -Dexe=$<TARGET_FILE:mini_usb_pass>
      -Dmax=${MAX_EXE_BYTES}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_size.cmake
    COMMENT "Checking mini_usb_pass.exe <= 100 KB"
    VERBATIM
  )
endif()
