#include "wall_of_flippers.h"
#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/utils.h"
#include <NimBLEDevice.h>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// ===================== GLOBALES & STRUCTURES =====================
struct ForbiddenPacket {
    const char *pattern;
    const char *type;
};

static const std::vector<ForbiddenPacket> rawForbiddenPackets = {
    {"4c0007190_______________00_____", "APPLE_DEVICE_POPUP"  },
    {"4c000f05c0_____________________", "APPLE_ACTION_MODAL"  },
    {"4c00071907_____________________", "APPLE_DEVICE_CONNECT"},
    {"4c0004042a0000000f05c1__604c950", "APPLE_DEVICE_SETUP"  },
    {"2cfe___________________________", "ANDROID_DEVICE_CONNECT"},
    {"750000000000000000000000000000_", "SAMSUNG_BUDS_POPUP"  },
    {"7500010002000101ff000043_______", "SAMSUNG_WATCH_PAIR"  },
    {"0600030080_____________________", "WINDOWS_SWIFT_PAIR"  },
    {"ff006db643ce97fe427c___________", "LOVE_TOYS"           }
};

struct ForbiddenPattern {
    uint8_t data[16];
    uint8_t mask[16];
    size_t length;
    const char* type;
};

static std::vector<ForbiddenPattern> preparsedPatterns;

struct WofItem {
    String name;
    String mac;
    String type;
    int8_t rssi;
    unsigned long lastSeen;
};

static std::vector<WofItem> wofItems;
static SemaphoreHandle_t wofMutex = nullptr;

// Pre-parsing patterns to speed up matching
void preparsePatterns() {
    preparsedPatterns.clear();
    for (const auto& p : rawForbiddenPackets) {
        ForbiddenPattern fp;
        fp.type = p.type;
        size_t plen = strlen(p.pattern);
        fp.length = plen / 2;
        if (fp.length > 16) fp.length = 16;

        for (size_t i = 0; i < fp.length; i++) {
            char byteStr[3] = {p.pattern[i*2], p.pattern[i*2+1], 0};
            if (byteStr[0] == '_' && byteStr[1] == '_') {
                fp.data[i] = 0;
                fp.mask[i] = 0;
            } else {
                fp.data[i] = (uint8_t)strtol(byteStr, nullptr, 16);
                fp.mask[i] = 0xFF;
            }
        }
        preparsedPatterns.push_back(fp);
    }
}

bool matchPreparsedPattern(const ForbiddenPattern& fp, const uint8_t* payload, size_t length) {
    if (length < fp.length) return false;
    for (size_t i = 0; i < fp.length; i++) {
        if ((payload[i] & fp.mask[i]) != fp.data[i]) return false;
    }
    return true;
}

class WofAdvertisedDeviceCallbacks : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice *advertisedDevice) override {
        const uint8_t *payload = advertisedDevice->getPayload().data();
        size_t length = advertisedDevice->getPayload().size();

        bool found = false;
        String type = "Unknown";
        for (const auto &fp : preparsedPatterns) {
            if (matchPreparsedPattern(fp, payload, length)) {
                found = true;
                type = fp.type;
                break;
            }
        }
        
        if (found && wofMutex) {
            if (xSemaphoreTake(wofMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                String mac = advertisedDevice->getAddress().toString().c_str();
                bool exists = false;
                for (auto &item : wofItems) {
                    if (item.mac == mac) {
                        item.lastSeen = millis();
                        item.rssi = advertisedDevice->getRSSI();
                        exists = true;
                        break;
                    }
                }
                if (!exists) {
                    WofItem newItem;
                    newItem.name = advertisedDevice->getName().c_str();
                    if (newItem.name.isEmpty()) newItem.name = "<no name>";
                    newItem.mac = mac;
                    newItem.type = type;
                    newItem.rssi = advertisedDevice->getRSSI();
                    newItem.lastSeen = millis();
                    wofItems.push_back(newItem);
                }
                xSemaphoreGive(wofMutex);
            }
        }
    }
};

void wall_of_flippers_setup() {
    if (!wofMutex) wofMutex = xSemaphoreCreateMutex();
    preparsePatterns();
    
    drawMainBorderWithTitle("Wall of Flippers");
    
    // Initial UI state
    tft.setTextSize(FP);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setCursor(10, 30);
    tft.printf("Scanning...");
    
    BLEDevice::init("");
    
    NimBLEScan *pBLEScan = BLEDevice::getScan();
    auto* callbacks = new WofAdvertisedDeviceCallbacks();
    pBLEScan->setScanCallbacks(callbacks, false);
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
    pBLEScan->start(0, false); // continuous scan

    unsigned long lastUpdate = 0;
    
    if (wofMutex) xSemaphoreTake(wofMutex, portMAX_DELAY);
    wofItems.clear();
    if (wofMutex) xSemaphoreGive(wofMutex);
    
    while (!check(EscPress)) {
        unsigned long now = millis();
        
        if (wofMutex && xSemaphoreTake(wofMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            // Purge old items (>15s)
            for (auto it = wofItems.begin(); it != wofItems.end(); ) {
                if (now - it->lastSeen > 15000) {
                    it = wofItems.erase(it);
                } else {
                    ++it;
                }
            }
            
            if (now - lastUpdate > 1000) {
                lastUpdate = now;
                // Only clear the list area (from y=30 downwards)
                tft.fillRect(5, 26, tftWidth - 10, tftHeight - 31, bruceConfig.bgColor);
                drawMainBorder(false); // redraw status bar and borders without clearing screen
                printTitle("Wall of Flippers");
                
                tft.setTextSize(FP);
                tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
                
                tft.setCursor(10, 30);
                tft.printf("Detected: %d", (int)wofItems.size());
                
                int y = 50;
                for (size_t i = 0; i < wofItems.size() && i < 4; ++i) {
                    tft.setCursor(10, y);
                    String line = wofItems[i].name + " [" + String(wofItems[i].rssi) + "]";
                    if (line.length() > 25) line = line.substring(0, 22) + "...";
                    tft.print(line);
                    y += 12;
                    tft.setCursor(15, y);
                    tft.setTextColor(TFT_DARKGREY, bruceConfig.bgColor);
                    tft.print(wofItems[i].type);
                    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
                    y += 15;
                }
            }
            xSemaphoreGive(wofMutex);
        }
        
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    
    pBLEScan->stop();
    pBLEScan->setScanCallbacks(nullptr, false);
    delete callbacks;
}
