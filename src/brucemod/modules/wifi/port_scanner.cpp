#include "port_scanner.h"
#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/utils.h"
#include <WiFi.h>
#include <WiFiClient.h>

static const int common_ports[] = {
    21, 22, 23, 25, 53, 80, 110, 135, 139, 143, 443, 445, 993, 995, 1723, 3306, 3389, 5900, 8080, 8443
};
static const int num_common_ports = sizeof(common_ports) / sizeof(common_ports[0]);

void port_scanner_setup() {
    if (WiFi.status() != WL_CONNECTED) {
        displayError("WiFi not connected");
        return;
    }

    String targetIP = keyboard("", 15, "Target IP:");
    if (targetIP == "\x1B" || targetIP.isEmpty()) return;

    IPAddress ip;
    if (!ip.fromString(targetIP)) {
        displayError("Invalid IP");
        return;
    }

    drawMainBorderWithTitle("Port Scanner");
    tft.setTextSize(FP);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setCursor(10, 30);
    tft.print("Scanning: ");
    tft.println(targetIP);
    
    int y = 50;
    int foundCount = 0;

    for (int i = 0; i < num_common_ports; i++) {
        if (check(EscPress)) break;

        int port = common_ports[i];
        tft.fillRect(100, 30, 100, 10, bruceConfig.bgColor);
        tft.setCursor(100, 30);
        tft.printf("Port: %d", port);

        WiFiClient client;
        if (client.connect(ip, port, 200)) { // 200ms timeout
            tft.setCursor(10, y);
            tft.printf("Port %d is OPEN", port);
            y += 12;
            foundCount++;
            client.stop();
        }
        
        if (y > tftHeight - 20) {
            // Simple overflow handling: just stop or clear
            // For now, just stop showing more
        }
        
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    tft.setCursor(10, tftHeight - 20);
    tft.printf("Done. Found: %d", foundCount);
    
    while (!check(AnyKeyPress)) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
