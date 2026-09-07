#pragma once
#include <HWCDC.h>

// USB-лог (на C3 Serial по умолчанию = UART0, поэтому свой HWCDC).
extern HWCDC usbLog;
#define LOG usbLog
