#include "wall_of_airtags.h"
#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/utils.h"
#include <NimBLEDevice.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

std::vector<AirTagItem> airTagItems;
static SemaphoreHandle_t airTagMutex = nullptr;

class AirTagAdvertisedDeviceCallbacks : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice *advertisedDevice) override {
        const uint8_t *payload = advertisedDevice->getPayload().data();
        size_t length = advertisedDevice->getPayload().size();

        // Apple FindMy pattern: 4C 00 12 19 ...
        bool isFindMy = false;
        if (length >= 6) {
            // Find manufacturer data
            for (size_t i = 0; i < length - 5; i++) {
                if (payload[i] == 0x4C && payload[i+1] == 0x00 && payload[i+2] == 0x12 && payload[i+3] == 0x19) {
                    isFindMy = true;
                    break;
                }
            }
        }

        if (isFindMy && airTagMutex) {
            if (xSemaphoreTake(airTagMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                String mac = advertisedDevice->getAddress().toString().c_str();
                String name = advertisedDevice->getName().c_str();
                int rssi = advertisedDevice->getRSSI();

                bool exists = false;
                for (auto &item : airTagItems) {
                    if (item.mac == mac) {
                        item.lastSeen = millis();
                        item.rssi = rssi;
                        exists = true;
                        break;
                    }
                }
                if (!exists) {
                    AirTagItem newItem;
                    newItem.mac = mac;
                    newItem.rssi = rssi;
                    newItem.lastSeen = millis();
                    newItem.name = name.isEmpty() ? "<no name>" : name;
                    airTagItems.push_back(newItem);
                }
                xSemaphoreGive(airTagMutex);
            }
        }
    }
};

void wall_of_airtags_setup() {
    if (!airTagMutex) airTagMutex = xSemaphoreCreateMutex();
    
    drawMainBorderWithTitle("Wall of AirTags");
    
    BLEDevice::init("");
    
    NimBLEScan *pBLEScan = BLEDevice::getScan();
    auto* callbacks = new AirTagAdvertisedDeviceCallbacks();
    pBLEScan->setScanCallbacks(callbacks, false);
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
    pBLEScan->start(0, false);

    unsigned long lastUpdate = 0;
    
    if (airTagMutex) xSemaphoreTake(airTagMutex, portMAX_DELAY);
    airTagItems.clear();
    if (airTagMutex) xSemaphoreGive(airTagMutex);
    
    while (!check(EscPress)) {
        unsigned long now = millis();
        
        if (airTagMutex && xSemaphoreTake(airTagMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            // Purge old items (>15s)
            for (auto it = airTagItems.begin(); it != airTagItems.end(); ) {
                if (now - it->lastSeen > 15000) {
                    it = airTagItems.erase(it);
                } else {
                    ++it;
                }
            }

            if (now - lastUpdate > 1000) {
                lastUpdate = now;
                tft.fillRect(5, 26, tftWidth - 10, tftHeight - 31, bruceConfig.bgColor);
                drawMainBorder(false);
                printTitle("Wall of AirTags");
                
                tft.setTextSize(FP);
                tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
                
                tft.setCursor(10, 30);
                tft.printf("AirTags: %d", (int)airTagItems.size());
                
                int y = 50;
                for (size_t i = 0; i < airTagItems.size() && i < 8; ++i) {
                    tft.setCursor(10, y);
                    String line = airTagItems[i].mac + " [" + String(airTagItems[i].rssi) + "]";
                    if (line.length() > 25) line = line.substring(0, 22) + "...";
                    tft.print(line);
                    y += 12;
                }
            }
            xSemaphoreGive(airTagMutex);
        }
        
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    
    pBLEScan->stop();
    pBLEScan->setScanCallbacks(nullptr, false);
    delete callbacks;
}
