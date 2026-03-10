#include "WifiMenu.h"
#include "core/display.h"
#include "core/settings.h"
#include "core/utils.h"
#include "core/wifi/webInterface.h"
#include "core/wifi/wg.h"
#include "core/wifi/wifi_common.h"
#include "core/wifi/wifi_mac.h"
#include "modules/ethernet/ARPScanner.h"
#include "modules/wifi/ap_info.h"
#include "modules/wifi/clients.h"
#include "modules/wifi/evil_portal.h"
#include "modules/wifi/karma_attack.h"
#include "modules/wifi/responder.h"
#include "modules/wifi/scan_hosts.h"
#include "modules/wifi/port_scanner.h"
#include "modules/wifi/bad_msg_attack.h"
#include "modules/wifi/packet_count.h"
#include "modules/wifi/sleep_attack.h"
#include "modules/wifi/sniffer.h"
#include "modules/wifi/wifi_atks.h"

#ifndef LITE_VERSION
#include "modules/wifi/wifi_recover.h"
#include "modules/pwnagotchi/pwnagotchi.h"
#endif

#include "modules/wifi/tcp_utils.h"

// global toggle - controls whether scanNetworks includes hidden SSIDs
bool showHiddenNetworks = false;

void WifiMenu::optionsMenu() {
    returnToMenu = false;
    options.clear();
    
    if (isWebUIActive) {
        drawMainBorderWithTitle("WiFi", true);
        padprintln("");
        padprintln("Starting a Wifi function will probably make the WebUI stop working");
        padprintln("");
        padprintln("Sel: to continue");
        padprintln("Any key: to Menu");
        while (1) {
            if (check(SelPress)) { break; }
            if (check(AnyKeyPress)) { return; }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
    }

    options = {
        {"Connectivity", [this]() { connectivityMenu(); }},
        {"Attacks",      [this]() { attacksMenu(); }},
        {"Sniffers",     [this]() { sniffersMenu(); }},
        {"Network Tools",[this]() { networkToolsMenu(); }},
        {"Config",       [this]() { configMenu(); }},
    };

    addOptionToMainMenu();
    loopOptions(options, MENU_TYPE_SUBMENU, "WiFi");
    options.clear();
}

void WifiMenu::connectivityMenu() {
    std::vector<Option> connOptions;
    if (WiFi.status() != WL_CONNECTED) {
        connOptions.push_back({"Connect to Wifi", lambdaHelper(wifiConnectMenu, WIFI_STA)});
        connOptions.push_back({"Start WiFi AP", [=]() {
                 wifiConnectMenu(WIFI_AP);
                 displayInfo("pwd: " + bruceConfig.wifiAp.pwd, true);
             }});
    }
    if (WiFi.getMode() != WIFI_MODE_NULL) { connOptions.push_back({"Turn Off WiFi", wifiDisconnect}); }
    if (WiFi.getMode() == WIFI_MODE_STA || WiFi.getMode() == WIFI_MODE_APSTA) {
        connOptions.push_back({"AP info", displayAPInfo});
    }
    connOptions.push_back({"Wireguard", wg_setup});
    connOptions.push_back({"Back", [this]() { optionsMenu(); }});
    loopOptions(connOptions, MENU_TYPE_SUBMENU, "Connectivity");
}

void WifiMenu::attacksMenu() {
    std::vector<Option> atkOptions;
    atkOptions.push_back({"Standard Attacks", wifi_atk_menu});
    atkOptions.push_back({"Evil Portal", [=]() {
                           if (isWebUIActive || server) {
                               stopWebUi();
                               wifiDisconnect();
                           }
                           EvilPortal();
                       }});
    atkOptions.push_back({"Bad Msg Attack", bad_msg_attack_setup});
    atkOptions.push_back({"Sleep Attack", sleep_attack_setup});
    atkOptions.push_back({"Responder", responder});
#ifndef LITE_VERSION
    atkOptions.push_back({"BruceModgotchi", brucemodgotchi_start});
    atkOptions.push_back({"WiFi Pass Recovery", wifi_recover_menu});
#endif
    atkOptions.push_back({"Back", [this]() { optionsMenu(); }});
    loopOptions(atkOptions, MENU_TYPE_SUBMENU, "Attacks");
}

void WifiMenu::sniffersMenu() {
    std::vector<Option> snifOptions;
    snifOptions.push_back({"Raw Sniffer", sniffer_setup});
    snifOptions.push_back({"Probe Sniffer", karma_setup});
    snifOptions.push_back({"Packet Count", packet_count_setup});
    snifOptions.push_back({"Back", [this]() { optionsMenu(); }});
    loopOptions(snifOptions, MENU_TYPE_SUBMENU, "Sniffers");
}

void WifiMenu::networkToolsMenu() {
    std::vector<Option> toolOptions;
    toolOptions.push_back({"Scan Hosts", [=]() {
                           bool doScan = true;
                           if (!wifiConnected) doScan = wifiConnectMenu();

                           if (doScan) {
                               esp_netif_t *esp_netinterface =
                                   esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
                               if (esp_netinterface == nullptr) {
                                   Serial.println("Failed to get netif handle");
                                   return;
                               }
                               ARPScanner{esp_netinterface};
                           }
                       }});
    toolOptions.push_back({"Port Scanner", port_scanner_setup});
    toolOptions.push_back({"Listen TCP", listenTcpPort});
    toolOptions.push_back({"Client TCP", clientTCP});
    toolOptions.push_back({"TelNET", telnet_setup});
    toolOptions.push_back({"SSH", lambdaHelper(ssh_setup, String(""))});
    toolOptions.push_back({"Back", [this]() { optionsMenu(); }});
    loopOptions(toolOptions, MENU_TYPE_SUBMENU, "Network Tools");
}

void WifiMenu::configMenu() {
    std::vector<Option> wifiOptions;

    wifiOptions.push_back({"Change MAC", wifiMACMenu});
    wifiOptions.push_back({"Add Evil Wifi", addEvilWifiMenu});
    wifiOptions.push_back({"Remove Evil Wifi", removeEvilWifiMenu});

    wifiOptions.push_back({"Evil Wifi Settings", [this]() {
                               std::vector<Option> evilOptions;

                               evilOptions.push_back({"Password Mode", setEvilPasswordMode});
                               evilOptions.push_back({"Rename /creds", setEvilEndpointCreds});
                               evilOptions.push_back({"Allow /creds access", setEvilAllowGetCreds});
                               evilOptions.push_back({"Rename /ssid", setEvilEndpointSsid});
                               evilOptions.push_back({"Allow /ssid access", setEvilAllowSetSsid});
                               evilOptions.push_back({"Display endpoints", setEvilAllowEndpointDisplay});
                               evilOptions.push_back({"Back", [this]() { configMenu(); }});
                               loopOptions(evilOptions, MENU_TYPE_SUBMENU, "Evil Wifi Settings");
                           }});

    {
        String hidden__wifi_option = String("Hidden Networks:") + (showHiddenNetworks ? "ON" : "OFF");
        Option opt(hidden__wifi_option.c_str(), [this]() {
            showHiddenNetworks = !showHiddenNetworks;
            displayInfo(String("Hidden Networks:") + (showHiddenNetworks ? "ON" : "OFF"), true);
            configMenu();
        });
        wifiOptions.push_back(opt);
    }
    wifiOptions.push_back({"Back", [this]() { optionsMenu(); }});
    loopOptions(wifiOptions, MENU_TYPE_SUBMENU, "WiFi Config");
}

void WifiMenu::drawIcon(float scale) {
    clearIconArea();
    int deltaY = scale * 20;
    int radius = scale * 6;

    tft.fillCircle(iconCenterX, iconCenterY + deltaY, radius, bruceConfig.priColor);
    tft.drawArc(
        iconCenterX,
        iconCenterY + deltaY,
        deltaY + radius,
        deltaY,
        130,
        230,
        bruceConfig.priColor,
        bruceConfig.bgColor
    );
    tft.drawArc(
        iconCenterX,
        iconCenterY + deltaY,
        2 * deltaY + radius,
        2 * deltaY,
        130,
        230,
        bruceConfig.priColor,
        bruceConfig.bgColor
    );
}
