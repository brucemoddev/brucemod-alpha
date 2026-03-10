#ifndef __MOUSE_JIGGLER_H__
#define __MOUSE_JIGGLER_H__

#include <Arduino.h>
#include <globals.h>
#include <Bad_Usb_Lib.h>

void mouse_jiggler_setup(HIDInterface *&hid, bool ble = false);

#endif
