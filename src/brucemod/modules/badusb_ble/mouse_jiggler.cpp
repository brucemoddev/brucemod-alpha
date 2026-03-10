#include "mouse_jiggler.h"
#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/utils.h"

#if defined(USB_as_HID)
#include "USBHIDMouse.h"
#include <USB.h>
#endif

void mouse_jiggler_setup(HIDInterface *&hid, bool ble) {
    if (ble) {
        displayError("BLE Mouse not supported yet");
        return;
    }

#if !defined(USB_as_HID) || !CONFIG_TINYUSB_HID_ENABLED
    displayError("USB HID Mouse not supported on this board");
    return;
#else
    USBHIDMouse Mouse;
    USB.begin();
    Mouse.begin();

    drawMainBorderWithTitle("Mouse Jiggler");
    tft.setTextSize(FP);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawCentreString("Jiggling...", tftWidth / 2, tftHeight / 2 - 10, 1);
    tft.drawCentreString("Press ESC to stop", tftWidth / 2, tftHeight - 20, 1);

    unsigned long lastMove = 0;
    int moveAmount = 10;
    bool direction = true;

    while (!check(EscPress)) {
        unsigned long now = millis();
        if (now - lastMove > 2000) {
            lastMove = now;
            if (direction) {
                Mouse.move(moveAmount, 0);
            } else {
                Mouse.move(-moveAmount, 0);
            }
            direction = !direction;
            
            // Visual feedback - alternate colors instead of blocking delay
            tft.fillCircle(tftWidth / 2, tftHeight / 2 + 20, 5, direction ? bruceConfig.priColor : bruceConfig.secColor);
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    
    Mouse.end();
#endif
}
