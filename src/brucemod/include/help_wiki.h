#ifndef __HELP_WIKI_H__
#define __HELP_WIKI_H__

#include <Arduino.h>

const char HELP_WIFI[] = 
"WiFi Module Help\n"
"Categories:\n"
"1. Connectivity: Connect to APs,\n"
"   setup AP or Wireguard.\n"
"2. Attacks: Standard (Deauth),\n"
"   Evil Portal, Bad Msg, Sleep,\n"
"   Responder, BruceModgotchi.\n"
"3. Sniffers: Raw, Probe (Karma),\n"
"   Packet Count stats.\n"
"4. Network Tools: ARP Scanner,\n"
"   Port Scanner, TelNet, SSH,\n"
"   TCP Client/Listen.\n"
"5. Config: MAC Spoofing,\n"
"   Evil WiFi settings.\n"
"\n"
"Shortcuts:\n"
"ESC: Stop/Back\n"
"1: This help menu";

const char HELP_BLE[] =
"BLE Module Help\n"
"Features:\n"
"1. Wall of Flippers: Detects\n"
"   Flipper Zero & more.\n"
"2. Wall of Airtags: Sniff FindMy.\n"
"3. Skimmer Det: Detect skimmers.\n"
"4. Spam: Target iOS, Windows,\n"
"   Android with pairing popups.\n"
"5. Airtag Spoof: Emulate tag.\n"
"\n"
"Shortcuts:\n"
"ESC: Stop/Back\n"
"Arrows: Scroll lists\n"
"1: This help menu";

const char HELP_FILES[] =
"Files Module Help\n"
"Features:\n"
"1. Enhanced Explorer: Icons,\n"
"   breadcrumbs & metadata.\n"
"2. Context Menu: Press [Opt]\n"
"   button for file/folder actions.\n"
"3. Recursive Copy: Copy folders\n"
"   between SD and LittleFS.\n"
"4. Mass Storage: USB Disk mode\n"
"   with live speed tracking.\n"
"\n"
"Shortcuts:\n"
"Opt: Context menu\n"
"ESC: Up dir/Back\n"
"1: This help menu";

const char HELP_IR[] =
"IR Module Help\n"
"Features:\n"
"1. TV-B-Gone: Multi-TV Off.\n"
"2. IR Receiver: Decode signals.\n"
"3. Custom IR: Load .ir files.\n"
"4. MakeHex: Universal encoder.\n"
"\n"
"Shortcuts:\n"
"ESC: Stop/Back\n"
"Enter: Transmit code\n"
"1: This help menu";

const char HELP_UI[] =
"UI Help\n"
"1. Main/Sub Modes: Set styles\n"
"   independently in Config.\n"
"2. Styles: List, Wii, DSi,\n"
"   PS4, Vertical, C64.\n"
"3. Full Labels: Untruncated\n"
"   text shown at screen bottom.\n"
"\n"
"Shortcuts:\n"
"1: This help menu";

inline const char* getHelpText(const char* title) {
    if (strstr(title, "WiFi")) return HELP_WIFI;
    if (strstr(title, "BLE") || strstr(title, "Bluetooth")) return HELP_BLE;
    if (strstr(title, "Explorer") || strstr(title, "Files")) return HELP_FILES;
    if (strstr(title, "IR") || strstr(title, "Infrared")) return HELP_IR;
    return HELP_UI;
}

#endif
