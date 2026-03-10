#include "skimmer_detector.h"
#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/utils.h"
#include <NimBLEDevice.h>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static const char *badDeviceNames[] = {"HC-03", "HC-05", "HC-06", "HC-08", "BT04-A", "BT05"};
static const int badDeviceNamesCount = sizeof(badDeviceNames) / sizeof(badDeviceNames[0]);

static const char *badMacPrefixes[] = {"00:11:22", "00:18:E4", "20:16:04"};
static const int badMacPrefixesCount = sizeof(badMacPrefixes) / sizeof(badMacPrefixes[0]);

struct SkimmerItem {
    String name;
    String mac;
    int8_t rssi;
    bool probable;
    unsigned long lastSeen;
};

static std::vector<SkimmerItem> skimmerItems;
static SemaphoreHandle_t skimmerMutex = nullptr;

class SkimmerAdvertisedDeviceCallbacks : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice *advertisedDevice) override {
        String mac = advertisedDevice->getAddress().toString().c_str();
        String name = advertisedDevice->getName().c_str();
        int rssi = advertisedDevice->getRSSI();

        bool isSkimmer = false;
        for (int i = 0; i < badDeviceNamesCount; i++) {
            if (name == badDeviceNames[i]) {
                isSkimmer = true;
                break;
            }
        }

        for (int i = 0; i < badMacPrefixesCount && !isSkimmer; i++) {
            if (mac.startsWith(badMacPrefixes[i])) {
                isSkimmer = true;
                break;
            }
        }

        if (skimmerMutex && xSemaphoreTake(skimmerMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            bool exists = false;
            for (auto &item : skimmerItems) {
                if (item.mac == mac) {
                    item.lastSeen = millis();
                    item.rssi = rssi;
                    item.probable = isSkimmer;
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                SkimmerItem newItem;
                newItem.name = name.isEmpty() ? "<no name>" : name;
                newItem.mac = mac;
                newItem.rssi = rssi;
                newItem.probable = isSkimmer;
                newItem.lastSeen = millis();
                skimmerItems.push_back(newItem);
            }
            xSemaphoreGive(skimmerMutex);
        }
    }
};

void skimmer_detector_setup() {
    if (!skimmerMutex) skimmerMutex = xSemaphoreCreateMutex();
    
    drawMainBorderWithTitle("Skimmer Detector");
    
    BLEDevice::init("");
    
    NimBLEScan *pBLEScan = BLEDevice::getScan();
    auto* callbacks = new SkimmerAdvertisedDeviceCallbacks();
    pBLEScan->setScanCallbacks(callbacks, false);
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
    pBLEScan->start(0, false);

    unsigned long lastUpdate = 0;
    
    if (skimmerMutex) xSemaphoreTake(skimmerMutex, portMAX_DELAY);
    skimmerItems.clear();
    if (skimmerMutex) xSemaphoreGive(skimmerMutex);
    
    while (!check(EscPress)) {
        unsigned long now = millis();
        
        if (skimmerMutex && xSemaphoreTake(skimmerMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            // Purge old items (>15s)
            for (auto it = skimmerItems.begin(); it != skimmerItems.end(); ) {
                if (now - it->lastSeen > 15000) {
                    it = skimmerItems.erase(it);
                } else {
                    ++it;
                }
            }

            if (now - lastUpdate > 1000) {
                lastUpdate = now;
                tft.fillRect(5, 26, tftWidth - 10, tftHeight - 31, bruceConfig.bgColor);
                drawMainBorder(false);
                printTitle("Skimmer Detector");
                
                tft.setTextSize(FP);
                
                int y = 30;
                int probableCount = 0;
                for (const auto &item : skimmerItems) {
                    if (item.probable) probableCount++;
                }
                
                tft.setTextColor(probableCount > 0 ? TFT_RED : bruceConfig.priColor, bruceConfig.bgColor);
                tft.setCursor(10, y);
                tft.printf("Probable Skimmers: %d", probableCount);
                y += 15;
                
                int count = 0;
                for (size_t i = 0; i < skimmerItems.size() && count < 6; ++i) {
                    if (skimmerItems[i].probable) {
                        tft.setTextColor(TFT_RED, bruceConfig.bgColor);
                    } else {
                        tft.setTextColor(TFT_DARKGREY, bruceConfig.bgColor);
                    }
                    
                    tft.setCursor(10, y);
                    String line = skimmerItems[i].name + " (" + String(skimmerItems[i].rssi) + ")";
                    if (line.length() > 25) line = line.substring(0, 22) + "...";
                    tft.print(line);
                    y += 12;
                    count++;
                }
            }
            xSemaphoreGive(skimmerMutex);
        }
        
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    
    pBLEScan->stop();
    pBLEScan->setScanCallbacks(nullptr, false);
    delete callbacks;
}
