#include "custom_ir.h"
#include "TV-B-Gone.h" // for checkIrTxPin()
#include "MakeHex.h"
#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/sd_functions.h"
#include "core/settings.h"
#include "core/type_convertion.h"
#include "ir_utils.h"
#include <IRutils.h>
#include <memory>

uint32_t swap32(uint32_t value) {
    return ((value & 0x000000FF) << 24) | ((value & 0x0000FF00) << 8) | ((value & 0x00FF0000) >> 8) |
           ((value & 0xFF000000) >> 24);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Custom IR

static std::vector<IRCode *> codes;

void resetCodesArray() {
    for (auto code : codes) { delete code; }
    codes.clear();
}

static std::vector<IRCode *> recent_ircodes;

void addToRecentCodes(IRCode *ircode) {
    for (auto recent_ircode : recent_ircodes) {
        if (recent_ircode->filepath == ircode->filepath && recent_ircode->name == ircode->name) { return; }
    }

    IRCode *ircode_copy = new IRCode(ircode);
    recent_ircodes.insert(recent_ircodes.begin(), ircode_copy);

    if (recent_ircodes.size() > 16) {
        delete recent_ircodes.back();
        recent_ircodes.pop_back();
    }
}

void selectRecentIrMenu() {
    checkIrTxPin();
    options = {};
    bool exit = false;
    IRCode *selected_code = NULL;
    for (auto recent_ircode : recent_ircodes) {
        if (recent_ircode->name == "") continue;
        options.push_back({recent_ircode->name.c_str(), [recent_ircode, &selected_code]() {
                               selected_code = recent_ircode;
                           }});
    }
    options.push_back({"Main Menu", [&]() { exit = true; }});

    int idx = 0;
    while (1) {
        idx = loopOptions(options, idx);
        if (selected_code != NULL) {
            sendIRCommand(selected_code);
            selected_code = NULL;
        }
        if (check(EscPress) || exit) break;
    }
    options.clear();
}

bool txIrFile(FS *fs, String filepath, bool hideDefaultUI) {
    File databaseFile = fs->open(filepath, FILE_READ);
    if (!databaseFile) {
        displayError("Fail to open file");
        return false;
    }

    setup_ir_pin(bruceConfigPins.irTx, OUTPUT);

    // One-pass parsing and sending
    String line;
    int total_codes = 0;
    // We don't know total_codes without first pass, but we can skip it for efficiency
    // and just show "Sending..." if we want speed. 
    // Or do a quick pass just for 'type:'
    while (databaseFile.available()) {
        line = databaseFile.readStringUntil('\n');
        if (line.startsWith("type:")) total_codes++;
    }
    databaseFile.seek(0);

    int codes_sent = 0;
    IRCode currentCode;
    bool inParsed = false;
    bool inRaw = false;

    while (databaseFile.available()) {
        line = databaseFile.readStringUntil('\n');
        line.trim();
        if (line.isEmpty()) continue;

        if (line.startsWith("type:")) {
            if (codes_sent > 0) {
                // Should we send the previous one? 
                // The Flipper format usually has name, type, and then data fields.
            }
            String type = line.substring(5);
            type.trim();
            currentCode = IRCode();
            currentCode.type = type;
            inParsed = (type == "parsed");
            inRaw = (type == "raw");
            codes_sent++;
            if (!hideDefaultUI) progressHandler(codes_sent, total_codes);
        } else if (line.startsWith("name:")) {
            currentCode.name = line.substring(5);
            currentCode.name.trim();
        } else if (line.startsWith("protocol:")) {
            currentCode.protocol = line.substring(9);
            currentCode.protocol.trim();
        } else if (line.startsWith("address:")) {
            currentCode.address = line.substring(8);
            currentCode.address.trim();
        } else if (line.startsWith("command:")) {
            currentCode.command = line.substring(8);
            currentCode.command.trim();
        } else if (line.startsWith("frequency:")) {
            currentCode.frequency = line.substring(10).toInt();
        } else if (line.startsWith("bits:")) {
            currentCode.bits = line.substring(5).toInt();
        } else if (line.startsWith("data:") || line.startsWith("value:") || line.startsWith("state:")) {
            currentCode.data = line.substring(line.indexOf(":") + 1);
            currentCode.data.trim();
            // In Flipper format, data/value is often the last field before # or next type:
            sendIRCommand(&currentCode, hideDefaultUI);
        }

        if (check(EscPress)) break;
    }

    databaseFile.close();
    digitalWrite(bruceConfigPins.irTx, LED_OFF);
    return true;
}

void otherIRcodes() {
    checkIrTxPin();
    resetCodesArray();
    String filepath;
    FS *fs = NULL;

    options = {
        {"Recent",   selectRecentIrMenu       },
        {"LittleFS", [&]() { fs = &LittleFS; }},
        {"Menu",     yield                    },
    };
    if (setupSdCard()) options.insert(options.begin(), {"SD Card", [&]() { fs = &SD; }});

    loopOptions(options);
    if (fs == NULL) return;

    if (!(*fs).exists("/BruceMod/IR")) (*fs).mkdir("/BruceMod/IR");
    filepath = loopSD(*fs, true, "IR", "/BruceMod/IR");
    if (filepath == "") return;

    bool mode_cmd = true;
    bool exit_sub = false;
    options = {
        {"Choose cmd", [&]() { mode_cmd = true; } },
        {"Spam all",   [&]() { mode_cmd = false; }},
        {"Back",       [&]() { exit_sub = true; } },
    };

    loopOptions(options);
    if (exit_sub) return;

    if (mode_cmd) chooseCmdIrFile(fs, filepath);
    else txIrFile(fs, filepath);
}

void sendIRCommand(IRCode *code, bool hideDefaultUI) {
    if (code->type == "") return;
    setup_ir_pin(bruceConfigPins.irTx, OUTPUT);

    if (code->type.equalsIgnoreCase("raw")) {
        sendRawCommand(code->frequency, code->data, hideDefaultUI);
    } else if (code->protocol.equalsIgnoreCase("NEC")) {
        sendNECCommand(code->address, code->command, hideDefaultUI);
    } else if (code->protocol.equalsIgnoreCase("NECext")) {
        sendNECextCommand(code->address, code->command, hideDefaultUI);
    } else if (code->protocol.equalsIgnoreCase("RC5") || code->protocol.equalsIgnoreCase("RC5X")) {
        sendRC5Command(code->address, code->command, hideDefaultUI);
    } else if (code->protocol.equalsIgnoreCase("RC6")) {
        sendRC6Command(code->address, code->command, hideDefaultUI);
    } else if (code->protocol.equalsIgnoreCase("Samsung32")) {
        sendSamsungCommand(code->address, code->command, hideDefaultUI);
    } else if (code->protocol.equalsIgnoreCase("SIRC")) {
        sendSonyCommand(code->address, code->command, 12, hideDefaultUI);
    } else if (code->protocol.equalsIgnoreCase("SIRC15")) {
        sendSonyCommand(code->address, code->command, 15, hideDefaultUI);
    } else if (code->protocol.equalsIgnoreCase("SIRC20")) {
        sendSonyCommand(code->address, code->command, 20, hideDefaultUI);
    } else if (code->protocol.equalsIgnoreCase("Kaseikyo")) {
        sendKaseikyoCommand(code->address, code->command, hideDefaultUI);
    } else {
        // Try native IRremoteESP8266 protocols
        decode_type_t type = strToDecodeType(code->protocol.c_str());
        if (type != decode_type_t::UNKNOWN) {
            sendDecodedCommand(code->protocol, code->data, code->bits, hideDefaultUI);
        } else {
            // Fallback to MakeHex
            int freq = 38000;
            RemoteCommand cmd;
            cmd.device = 0; cmd.subdevice = -1; cmd.function = 0;
            
            if (code->address.indexOf(".") > 0) {
                cmd.device = code->address.substring(0, code->address.indexOf(".")).toInt();
                cmd.subdevice = code->address.substring(code->address.indexOf(".") + 1).toInt();
            } else {
                cmd.device = strtol(code->address.c_str(), nullptr, 0);
            }
            cmd.function = strtol(code->command.c_str(), nullptr, 0);
            
            std::vector<float> seq = MakeHex::encodeRemoteCommand(cmd, code->protocol.c_str(), freq);
            if (!seq.empty()) {
                String rawStr = "";
                for (size_t i = 0; i < seq.size(); i++) {
                    rawStr += String((int)seq[i]) + (i == seq.size() - 1 ? "" : ",");
                }
                sendRawCommand(freq / 1000, rawStr, hideDefaultUI);
            } else {
                Serial.println("Protocol not supported: " + code->protocol);
                if (!hideDefaultUI) displayError("Unsupported Protocol");
            }
        }
    }
}

// ... sendNECCommand, sendNECextCommand, sendRC5Command, sendRC6Command, sendSamsungCommand, sendSonyCommand, sendKaseikyoCommand, sendDecodedCommand ...
// (Keeping existing implementations for these as they seem to work with the library)

void sendNECCommand(String address, String command, bool hideDefaultUI) {
    IRsend irsend(bruceConfigPins.irTx);
    irsend.begin();
    if (!hideDefaultUI) { displayTextLine("Sending.."); }
    uint32_t addressValue = strtoul(address.c_str(), nullptr, 16);
    uint32_t commandValue = strtoul(command.c_str(), nullptr, 16);
    irsend.sendNEC(irsend.encodeNEC(addressValue, commandValue), 32);
    if (bruceConfigPins.irTxRepeats > 0) {
        for (uint8_t i = 0; i < bruceConfigPins.irTxRepeats; i++) irsend.sendNEC(irsend.encodeNEC(addressValue, commandValue), 32);
    }
    digitalWrite(bruceConfigPins.irTx, LED_OFF);
}

void sendNECextCommand(String address, String command, bool hideDefaultUI) {
    IRsend irsend(bruceConfigPins.irTx);
    irsend.begin();
    if (!hideDefaultUI) { displayTextLine("Sending.."); }
    uint32_t addressValue = strtoul(address.c_str(), nullptr, 16);
    uint32_t commandValue = strtoul(command.c_str(), nullptr, 16);
    // NECext usually means 16-bit address and 8-bit command + inverse
    irsend.sendNEC(irsend.encodeNEC(addressValue, commandValue), 32);
    digitalWrite(bruceConfigPins.irTx, LED_OFF);
}

void sendRC5Command(String address, String command, bool hideDefaultUI) {
    IRsend irsend(bruceConfigPins.irTx, true);
    irsend.begin();
    uint32_t addr = strtoul(address.c_str(), nullptr, 16);
    uint32_t cmd = strtoul(command.c_str(), nullptr, 16);
    irsend.sendRC5(irsend.encodeRC5(addr, cmd), 12);
    digitalWrite(bruceConfigPins.irTx, LED_OFF);
}

void sendRC6Command(String address, String command, bool hideDefaultUI) {
    IRsend irsend(bruceConfigPins.irTx, true);
    irsend.begin();
    uint32_t addr = strtoul(address.c_str(), nullptr, 16);
    uint32_t cmd = strtoul(command.c_str(), nullptr, 16);
    irsend.sendRC6(irsend.encodeRC6(addr, cmd), 20);
    digitalWrite(bruceConfigPins.irTx, LED_OFF);
}

void sendSamsungCommand(String address, String command, bool hideDefaultUI) {
    IRsend irsend(bruceConfigPins.irTx);
    irsend.begin();
    uint32_t addr = strtoul(address.c_str(), nullptr, 16);
    uint32_t cmd = strtoul(command.c_str(), nullptr, 16);
    irsend.sendSAMSUNG(irsend.encodeSAMSUNG(addr, cmd), 32);
    digitalWrite(bruceConfigPins.irTx, LED_OFF);
}

void sendSonyCommand(String address, String command, uint8_t nbits, bool hideDefaultUI) {
    IRsend irsend(bruceConfigPins.irTx);
    irsend.begin();
    uint32_t addr = strtoul(address.c_str(), nullptr, 16);
    uint32_t cmd = strtoul(command.c_str(), nullptr, 16);
    irsend.sendSony(cmd, nbits, 2); // Sony expects command then address usually handled by sendSony
    digitalWrite(bruceConfigPins.irTx, LED_OFF);
}

void sendKaseikyoCommand(String address, String command, bool hideDefaultUI) {
    // Simplified, ideally use irsend.sendPanasonic
    IRsend irsend(bruceConfigPins.irTx);
    irsend.begin();
    uint64_t addr = strtoull(address.c_str(), nullptr, 16);
    uint64_t cmd = strtoull(command.c_str(), nullptr, 16);
    irsend.sendPanasonic(addr, cmd);
    digitalWrite(bruceConfigPins.irTx, LED_OFF);
}

bool sendDecodedCommand(String protocol, String value, uint8_t bits, bool hideDefaultUI) {
    decode_type_t type = strToDecodeType(protocol.c_str());
    if (type == decode_type_t::UNKNOWN) return false;
    IRsend irsend(bruceConfigPins.irTx);
    irsend.begin();
    if (hasACState(type)) {
        // ... (state parsing logic remains same but improved)
        std::vector<uint8_t> state;
        for (uint16_t i = 0; i < value.length(); i += 3) {
            state.push_back((hexCharToDecimal(value[i]) << 4) | hexCharToDecimal(value[i + 1]));
        }
        irsend.send(type, state.data(), state.size());
    } else {
        uint64_t val = strtoull(value.c_str(), nullptr, 16);
        irsend.send(type, val, bits);
    }
    digitalWrite(bruceConfigPins.irTx, LED_OFF);
    return true;
}

void sendRawCommand(uint16_t frequency, String rawData, bool hideDefaultUI) {
    IRsend irsend(bruceConfigPins.irTx);
    irsend.begin();
    if (!hideDefaultUI) { displayTextLine("Sending RAW.."); }

    std::vector<uint16_t> buffer;
    char *ptr = const_cast<char*>(rawData.c_str());
    while (*ptr) {
        while (*ptr && !isdigit(*ptr)) ptr++;
        if (!*ptr) break;
        buffer.push_back((uint16_t)strtoul(ptr, &ptr, 10));
    }

    if (!buffer.empty()) {
        irsend.sendRaw(buffer.data(), buffer.size(), frequency);
        if (bruceConfigPins.irTxRepeats > 0) {
            for (uint8_t i = 0; i < bruceConfigPins.irTxRepeats; i++) {
                irsend.sendRaw(buffer.data(), buffer.size(), frequency);
            }
        }
    }
    digitalWrite(bruceConfigPins.irTx, LED_OFF);
}

bool chooseCmdIrFile(FS *fs, String filepath) {
    checkIrTxPin();
    resetCodesArray();
    File databaseFile = fs->open(filepath, FILE_READ);
    if (!databaseFile) return false;

    String line;
    IRCode *current = nullptr;
    while (databaseFile.available()) {
        line = databaseFile.readStringUntil('\n');
        line.trim();
        if (line.isEmpty()) continue;

        if (line.startsWith("name:")) {
            current = new IRCode();
            current->name = line.substring(5);
            current->name.trim();
            current->filepath = filepath;
            codes.push_back(current);
        } else if (current) {
            if (line.startsWith("type:")) current->type = line.substring(5);
            else if (line.startsWith("protocol:")) current->protocol = line.substring(9);
            else if (line.startsWith("address:")) current->address = line.substring(8);
            else if (line.startsWith("command:")) current->command = line.substring(8);
            else if (line.startsWith("frequency:")) current->frequency = line.substring(10).toInt();
            else if (line.startsWith("bits:")) current->bits = line.substring(5).toInt();
            else if (line.startsWith("data:") || line.startsWith("value:") || line.startsWith("state:")) {
                current->data = line.substring(line.indexOf(":") + 1);
                current->data.trim();
            }
        }
        if (codes.size() >= 150) break; 
    }
    databaseFile.close();

    options.clear();
    for (auto code : codes) {
        options.push_back({code->name.c_str(), [code]() {
            sendIRCommand(code);
            addToRecentCodes(code);
        }});
    }
    options.push_back({"Back", [](){}});

    int idx = 0;
    while (1) {
        idx = loopOptions(options, idx);
        if (check(EscPress) || idx == -1 || idx == (int)options.size() - 1) break;
    }
    resetCodesArray();
    return true;
}
