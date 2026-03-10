#include "rf_utils.h"
#include "core/settings.h"

// CRC-64-ECMA constants
const uint64_t CRC64_ECMA_POLY = 0x42F0E1EBA9EA3693; // Polynomial for CRC-64-ECMA
const uint64_t CRC64_ECMA_INIT = 0xFFFFFFFFFFFFFFFF; // Initial value

// Compile-time CRC64 table generation
template<size_t N>
struct CRC64Table {
    uint64_t values[N];
    constexpr CRC64Table() : values{} {
        for (size_t i = 0; i < N; ++i) {
            uint64_t crc = i;
            for (int j = 0; j < 64; ++j) {
                if (crc & 1) {
                    crc = (crc >> 1) ^ 0x9A6C9329AC4BC9B5; // Reflected POLY for right-shift
                } else {
                    crc >>= 1;
                }
            }
            values[i] = crc;
        }
    }
};

// However, the current implementation is NOT reflected and uses high-byte XOR.
// To keep it simple and CPU-efficient without changing logic:
template<size_t N>
struct CRC64TableStandard {
    uint64_t values[N];
    constexpr CRC64TableStandard() : values{} {
        for (size_t i = 0; i < N; ++i) {
            uint64_t crc = (uint64_t)i << 56;
            for (int j = 0; j < 8; ++j) {
                if (crc & 0x8000000000000000) {
                    crc = (crc << 1) ^ 0x42F0E1EBA9EA3693;
                } else {
                    crc <<= 1;
                }
            }
            values[i] = crc;
        }
    }
};
static constexpr auto crc64_table = CRC64TableStandard<256>();

const int range_limits[4][2] = {
    {0,  23}, // 300-348 MHz
    {24, 47}, // 387-464 MHz
    {48, 56}, // 779-928 MHz
    {0,  56}  // All ranges
};
const char *subghz_frequency_ranges[] = {"300-348 MHz", "387-464 MHz", "779-928 MHz", "All ranges"};
const float subghz_frequency_list[] = {
    /* 300 - 348 MHz Frequency Range */
    300.000f, 302.757f, 303.875f, 303.900f, 304.250f, 307.000f, 307.500f, 307.800f, 
    309.000f, 310.000f, 312.000f, 312.100f, 312.200f, 313.000f, 313.850f, 314.000f, 
    314.350f, 314.980f, 315.000f, 318.000f, 330.000f, 345.000f, 348.000f, 350.000f,
    /* 387 - 464 MHz Frequency Range */
    387.000f, 390.000f, 418.000f, 430.000f, 430.500f, 431.000f, 431.500f, 433.075f, 
    433.220f, 433.420f, 433.657f, 433.889f, 433.920f, 434.075f, 434.177f, 434.190f, 
    434.390f, 434.420f, 434.620f, 434.775f, 438.900f, 440.175f, 464.000f, 467.750f,
    /* 779 - 928 MHz Frequency Range */
    779.000f, 868.350f, 868.400f, 868.800f, 868.950f, 906.400f, 915.000f, 925.000f, 
    928.000f
};

RfCodes recent_rfcodes[16];
int recent_rfcodes_last_used = 0;
bool rmtInstalled = true;
static bool cc1101_spi_ready = false;

// Function to compute CRC-64-ECMA using precomputed table
uint64_t crc64_ecma(const std::vector<int> &data) {
    uint64_t crc = CRC64_ECMA_INIT;
    for (int value : data) {
        // Current implementation treats each 'int' as a single XOR step on the high byte
        uint8_t byte = (uint8_t)(value & 0xFF);
        crc = (crc << 8) ^ crc64_table.values[((crc >> 56) ^ byte) & 0xFF];
    }
    return crc;
}

bool initRfModule(String mode, float frequency) {
    if (!frequency) frequency = bruceConfigPins.rfFreq;
    if (bruceConfigPins.rfModule == CC1101_SPI_MODULE) {
        if (bruceConfigPins.CC1101_bus.mosi == (gpio_num_t)TFT_MOSI &&
            bruceConfigPins.CC1101_bus.mosi != GPIO_NUM_NC) {
#if TFT_MOSI > 0
            initCC1101once(&tft.getSPIinstance());
#else
            yield();
#endif
        } else if (bruceConfigPins.CC1101_bus.mosi == bruceConfigPins.SDCARD_bus.mosi) {
            initCC1101once(&sdcardSPI);
        } else if (bruceConfigPins.NRF24_bus.mosi == bruceConfigPins.CC1101_bus.mosi &&
                   bruceConfigPins.CC1101_bus.mosi != bruceConfigPins.SDCARD_bus.mosi) {
            CC_NRF_SPI.end();
            delay(10);
            if (!CC_NRF_SPI.begin(bruceConfigPins.CC1101_bus.sck, bruceConfigPins.CC1101_bus.miso, bruceConfigPins.CC1101_bus.mosi)) {
                Serial.println("Failed to start CC1101 SPI on NRF24 pins!");
            }
            initCC1101once(&CC_NRF_SPI);
        } else {
            ELECHOUSE_cc1101.setBeginEndLogic(true);
            initCC1101once(NULL);
        }
        ELECHOUSE_cc1101.Init();
        if (ELECHOUSE_cc1101.getCC1101()) {
            Serial.println("cc1101 Connection OK");
        } else {
            displayError("CC1101 not found");
            return false;
        }
        if (!((frequency >= 280 && frequency <= 350) || (frequency >= 387 && frequency <= 468) || (frequency >= 779 && frequency <= 928))) {
            frequency = 433.92;
            displayWarning("Wrong freq, set to 433.92", true);
        }
        ELECHOUSE_cc1101.setRxBW(256);
        ELECHOUSE_cc1101.setClb(1, 13, 15);
        ELECHOUSE_cc1101.setClb(2, 16, 19);
        ELECHOUSE_cc1101.setModulation(2);
        ELECHOUSE_cc1101.setDRate(50);
        ELECHOUSE_cc1101.setPktFormat(3);
        setMHZ(frequency);
        if (mode == "tx") {
            ioExpander.turnPinOnOff(IO_EXP_CC_RX, LOW);
            ioExpander.turnPinOnOff(IO_EXP_CC_TX, HIGH);
            pinMode(bruceConfigPins.CC1101_bus.io0, OUTPUT);
            ELECHOUSE_cc1101.setPA(12);
            ELECHOUSE_cc1101.SetTx();
        } else if (mode == "rx") {
            ioExpander.turnPinOnOff(IO_EXP_CC_RX, HIGH);
            ioExpander.turnPinOnOff(IO_EXP_CC_TX, LOW);
            pinMode(bruceConfigPins.CC1101_bus.io0, INPUT);
            ELECHOUSE_cc1101.SetRx();
        }
        cc1101_spi_ready = true;
    } else {
        if (mode == "tx") {
            gsetRfTxPin(false);
            if (bruceConfigPins.SDCARD_bus.checkConflict(bruceConfigPins.rfTx)) sdcardSPI.end();
            gpio_reset_pin((gpio_num_t)bruceConfigPins.rfTx);
            pinMode(bruceConfigPins.rfTx, OUTPUT);
            digitalWrite(bruceConfigPins.rfTx, LOW);
        } else if (mode == "rx") {
            gsetRfRxPin(false);
            if (bruceConfigPins.SDCARD_bus.checkConflict(bruceConfigPins.rfRx)) sdcardSPI.end();
            gpio_reset_pin((gpio_num_t)bruceConfigPins.rfRx);
            pinMode(bruceConfigPins.rfRx, INPUT);
        }
    }
    return true;
}

void deinitRfModule() {
    if (bruceConfigPins.rfModule == CC1101_SPI_MODULE) {
        if (cc1101_spi_ready) {
            ELECHOUSE_cc1101.setSidle();
            cc1101_spi_ready = false;
        }
        digitalWrite(bruceConfigPins.CC1101_bus.io0, LOW);
        digitalWrite(bruceConfigPins.CC1101_bus.cs, HIGH);
        ioExpander.turnPinOnOff(IO_EXP_CC_RX, LOW);
        ioExpander.turnPinOnOff(IO_EXP_CC_TX, LOW);
    } else digitalWrite(bruceConfigPins.rfTx, LED_OFF);
}

void initCC1101once(SPIClass *SSPI) {
    if (SSPI != NULL) ELECHOUSE_cc1101.setSPIinstance(SSPI);
    else ELECHOUSE_cc1101.setSPIinstance(nullptr);
    ELECHOUSE_cc1101.setSpiPin(bruceConfigPins.CC1101_bus.sck, bruceConfigPins.CC1101_bus.miso, bruceConfigPins.CC1101_bus.mosi, bruceConfigPins.CC1101_bus.cs);
    ELECHOUSE_cc1101.setGDO0(bruceConfigPins.CC1101_bus.io0);
}

void setMHZ(float frequency) {
    if (frequency > 928 || frequency < 280) frequency = 433.92;
    if (bruceConfigPins.rfModule == CC1101_SPI_MODULE) {
#if defined(T_EMBED)
        static uint8_t antenna = 200;
        bool change = true;
#if !defined(T_EMBED_1101)
        if (bruceConfigPins.CC1101_bus.cs != 17) change = false;
#endif
        if (frequency <= 350 && antenna != 0 && change) {
            digitalWrite(CC1101_SW1_PIN, HIGH); digitalWrite(CC1101_SW0_PIN, LOW); antenna = 0;
            vTaskDelay(10 / portTICK_PERIOD_MS);
        } else if (frequency > 350 && frequency < 468 && antenna != 1 && change) {
            digitalWrite(CC1101_SW1_PIN, HIGH); digitalWrite(CC1101_SW0_PIN, HIGH); antenna = 1;
            vTaskDelay(10 / portTICK_PERIOD_MS);
        } else if (frequency > 778 && antenna != 2 && change) {
            digitalWrite(CC1101_SW1_PIN, LOW); digitalWrite(CC1101_SW0_PIN, HIGH); antenna = 2;
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
#endif
        ELECHOUSE_cc1101.setMHZ(frequency);
    }
}

int find_pulse_index(const std::vector<int> &indexed_durations, int duration) {
    int abs_duration = abs(duration);
    int closest_index = -1;
    int closest_diff = 999999;
    for (size_t i = 0; i < indexed_durations.size(); i++) {
        int diff = abs(indexed_durations[i] - abs_duration);
        if (diff <= 50) return i;
        if (diff < closest_diff) { closest_diff = diff; closest_index = i; }
    }
    if (indexed_durations.size() < 4) return -1;
    return closest_index;
}

void addToRecentCodes(struct RfCodes rfcode) {
    recent_rfcodes[recent_rfcodes_last_used] = rfcode;
    recent_rfcodes_last_used += 1;
    if (recent_rfcodes_last_used == 16) recent_rfcodes_last_used = 0;
}

struct RfCodes selectRecentRfMenu() {
    options = {};
    bool exit = false;
    struct RfCodes selected_code;
    for (int i = 0; i < 16; i++) {
        if (recent_rfcodes[i].filepath == "") continue;
        options.emplace_back(recent_rfcodes[i].filepath.c_str(), [i, &selected_code]() {
            selected_code = recent_rfcodes[i];
        });
    }
    options.emplace_back("Main Menu", [&]() { exit = true; });
    loopOptions(options);
    options.clear();
    return selected_code;
}

rmt_channel_handle_t setup_rf_rx() {
    if (!initRfModule("rx", bruceConfigPins.rfFreq)) return NULL;
    setMHZ(bruceConfigPins.rfFreq);
    rmt_rx_channel_config_t rx_channel_cfg = {};
    rx_channel_cfg.gpio_num = bruceConfigPins.rfModule == CC1101_SPI_MODULE ? gpio_num_t(bruceConfigPins.CC1101_bus.io0) : gpio_num_t(bruceConfigPins.rfRx);
    rx_channel_cfg.clk_src = RMT_CLK_SRC_DEFAULT;
    rx_channel_cfg.resolution_hz = 1 * 1000 * 1000;
    rx_channel_cfg.mem_block_symbols = 64;
    rx_channel_cfg.intr_priority = 0;
    rx_channel_cfg.flags.invert_in = false;
    rx_channel_cfg.flags.with_dma = false;
    rx_channel_cfg.flags.allow_pd = false;
    rx_channel_cfg.flags.io_loop_back = false;
    rmt_channel_handle_t rx_channel = NULL;
    ESP_ERROR_CHECK(rmt_new_rx_channel(&rx_channel_cfg, &rx_channel));
    return rx_channel;
}

bool setMHZMenu() {
    if (bruceConfigPins.rfModule != CC1101_SPI_MODULE) return false;
    if (check(SelPress)) {
        options = {};
        int ind = 0;
        int arraySize = sizeof(subghz_frequency_list) / sizeof(subghz_frequency_list[0]);
        for (int i = 0; i < arraySize; i++) {
            if (subghz_frequency_list[i] - bruceConfigPins.rfFreq < 0.1) ind = i;
            String tmp = String(subghz_frequency_list[i], 2) + "Mhz";
            options.push_back({tmp.c_str(), [=]() { bruceConfigPins.rfFreq = subghz_frequency_list[i]; }});
        }
        loopOptions(options, ind);
        options.clear();
        setMHZ(bruceConfigPins.rfFreq);
        return true;
    }
    return false;
}

void rf_range_selection(float currentFrequency) {
    int option = 0;
    options = {
        {String("Fixed [" + String(bruceConfigPins.rfFreq) + "]").c_str(), [=]() { bruceConfigPins.setRfFreq(bruceConfigPins.rfFreq, 2); }},
        {String("Choose Fixed").c_str(), [&]() { option = 1; }},
        {subghz_frequency_ranges[0], [=]() { bruceConfigPins.setRfScanRange(0); }},
        {subghz_frequency_ranges[1], [=]() { bruceConfigPins.setRfScanRange(1); }},
        {subghz_frequency_ranges[2], [=]() { bruceConfigPins.setRfScanRange(2); }},
        {subghz_frequency_ranges[3], [=]() { bruceConfigPins.setRfScanRange(3); }},
    };
    loopOptions(options);
    options.clear();
    if (option == 1) {
        options = {};
        int ind = 0;
        int arraySize = sizeof(subghz_frequency_list) / sizeof(subghz_frequency_list[0]);
        for (int i = 0; i < arraySize; i++) {
            String tmp = String(subghz_frequency_list[i], 2) + "Mhz";
            options.push_back({tmp.c_str(), [=]() { bruceConfigPins.setRfFreq(subghz_frequency_list[i], 2); }});
            if (int(currentFrequency * 100) == int(subghz_frequency_list[i] * 100)) ind = i;
        }
        loopOptions(options, ind);
        options.clear();
    }
    if (bruceConfigPins.rfFxdFreq) displayTextLine("Scan freq set to " + String(bruceConfigPins.rfFreq));
    else displayTextLine("Range set to " + String(subghz_frequency_ranges[bruceConfigPins.rfScanRange]));
}
