#include "packet_count.h"
#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/utils.h"
#include <WiFi.h>
#include <esp_wifi.h>

static long mgmt_pkts = 0;
static long data_pkts = 0;
static long ctrl_pkts = 0;
static long other_pkts = 0;

void packet_sniffer_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (type == WIFI_PKT_MGMT) mgmt_pkts++;
    else if (type == WIFI_PKT_DATA) data_pkts++;
    else if (type == WIFI_PKT_CTRL) ctrl_pkts++;
    else other_pkts++;
}

void packet_count_setup() {
    mgmt_pkts = 0; data_pkts = 0; ctrl_pkts = 0; other_pkts = 0;

    // Ensure WiFi is in a safe state for promiscuous mode
    esp_wifi_set_promiscuous(false);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(packet_sniffer_cb);

    drawMainBorderWithTitle("Packet Count");
    tft.setTextSize(FM);
    
    unsigned long lastUpdate = 0;
    int current_chan = 1;

    while (!check(EscPress)) {
        unsigned long now = millis();
        
        if (now - lastUpdate > 800) {
            lastUpdate = now;
            
            // Channel hopping
            esp_wifi_set_channel(current_chan, WIFI_SECOND_CHAN_NONE);

            // Clear data area only to prevent flicker
            tft.fillRect(5, 26, tftWidth - 10, tftHeight - 31, bruceConfig.bgColor);
            drawMainBorder(false);
            printTitle("Packet Count");

            tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
            tft.setCursor(10, 35);
            tft.printf("CH: %d", current_chan);
            
            tft.setCursor(10, 60);
            tft.setTextColor(TFT_YELLOW, bruceConfig.bgColor);
            tft.printf("MGMT: %ld", mgmt_pkts);
            
            tft.setCursor(10, 80);
            tft.setTextColor(TFT_GREEN, bruceConfig.bgColor);
            tft.printf("DATA: %ld", data_pkts);
            
            tft.setCursor(10, 100);
            tft.setTextColor(TFT_CYAN, bruceConfig.bgColor);
            tft.printf("CTRL: %ld", ctrl_pkts);
            
            tft.setCursor(10, 120);
            tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
            tft.printf("Other: %ld", other_pkts);

            current_chan++;
            if (current_chan > 13) current_chan = 1;
        }
        
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    esp_wifi_set_promiscuous(false);
}
