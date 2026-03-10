#include "airtag_spoof.h"
#include "wall_of_airtags.h"
#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/utils.h"
#include <NimBLEDevice.h>
#include <esp_bt.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

extern "C" esp_err_t esp_base_mac_addr_set(const uint8_t *mac);

void airtag_spoof_setup() {
    if (airTagItems.empty()) {
        displayError("No AirTags detected yet. Run Sniffer first.");
        return;
    }

    std::vector<Option> tagOptions;
    std::vector<String> macs;
    for (const auto& item : airTagItems) {
        macs.push_back(item.mac);
    }

    for (size_t i = 0; i < macs.size(); i++) {
        tagOptions.push_back({macs[i], [](){}});
    }
    tagOptions.push_back({"Back", [](){}});

    int selected = loopOptions(tagOptions, MENU_TYPE_SUBMENU, "Select Tag to Spoof");
    if (selected < 0 || selected >= (int)macs.size()) return;

    String targetMac = macs[selected];
    uint8_t macBytes[6];
    int values[6];
    if (sscanf(targetMac.c_str(), "%x:%x:%x:%x:%x:%x", &values[0], &values[1], &values[2], &values[3], &values[4], &values[5]) == 6) {
        for (int i = 0; i < 6; i++) macBytes[i] = (uint8_t)values[i];
    } else return;

    uint32_t lastTwo = (macBytes[4] << 8) | macBytes[5];
    if (lastTwo >= 2) {
        lastTwo -= 2;
    } else {
        uint32_t high = (macBytes[0] << 24) | (macBytes[1] << 16) | (macBytes[2] << 8) | macBytes[3];
        high--; 
        macBytes[0] = (high >> 24) & 0xFF;
        macBytes[1] = (high >> 16) & 0xFF;
        macBytes[2] = (high >> 8) & 0xFF;
        macBytes[3] = high & 0xFF;
        lastTwo = 0xFFFF - (2 - lastTwo - 1);
    }
    macBytes[4] = (lastTwo >> 8) & 0xFF;
    macBytes[5] = lastTwo & 0xFF;

    drawMainBorderWithTitle("Airtag Spoofing");
    tft.setTextSize(FP);
    tft.setTextColor(TFT_RED, bruceConfig.bgColor);
    tft.drawCentreString("Spoofing: " + targetMac, tftWidth / 2, 50, 1);
    tft.drawCentreString("Press ESC to stop", tftWidth / 2, 80, 1);

    BLEDevice::deinit(true);

    esp_base_mac_addr_set(macBytes);
    BLEDevice::init("");
    NimBLEServer *pServer = BLEDevice::createServer();
    NimBLEAdvertising *pAdvertising = pServer->getAdvertising();

    uint8_t payload[] = {
        0x1e, 0xff, 0x4c, 0x00, 0x12, 0x19, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    
    NimBLEAdvertisementData advData;
    advData.addData(payload, sizeof(payload));
    pAdvertising->setAdvertisementData(advData);
    pAdvertising->start();

    while (!check(EscPress)) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    pAdvertising->stop();
    BLEDevice::deinit(true);
    
    displaySuccess("Spoofing stopped. Rebooting...");
    delay(1000);
    ESP.restart();
}
