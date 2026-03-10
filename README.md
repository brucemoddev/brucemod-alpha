![Bruce Main Menu](./media/pictures/bruce_banner.jpg)

# :shark: BruceMod (Independent Fork)

**BruceMod** is a versatile, independent ESP32 firmware fork based on the original Bruce project. It focuses on stability, enhanced UI, and advanced offensive features for Red Team operations using M5Stack and Lilygo hardware.

This project is an **independent community fork** and is not affiliated with the original pre-built tools or commercial offerings.

**Check our fully open-source hardware too:** https://bruce.computer/boards

## :rocket: Optimized Features

BruceMod includes several optimizations and UI improvements:
- **Independent Menu Styling**: Set different styles for the Main Menu and Submenus (e.g., PS4 style for main, C64 style for submenus).
- **Enhanced File Explorer**: A modern interface with icons, breadcrumbs, and live item metadata.
  - **Context Menu**: Use the **[Opt]** button (next to Ctrl) to open context menus for both files and folders.
  - **Recursive Copy/Paste**: Easily move entire folders and files between SD Card and LittleFS.
- **USB Mass Storage with Speedometer**: Real-time average speed monitoring (kB/s) updated every 20 seconds.
- **Improved Performance**: Pre-parsed BLE patterns and `constexpr` precomputations for IR/RF engines reduce CPU load.
- **Categorized WiFi Menu**: Organized into Connectivity, Attacks, Sniffers, Network Tools, and Config for faster access.

## :building_construction: How to install

### The easiest way to install BruceMod is using our official Web Flasher!
### Check out: https://bruce.computer/flasher

Alternatively, flash locally using esptool.py:
```sh
esptool.py --port /dev/ttyACM0 write_flash 0x00000 Bruce-<device>.bin
```

## :bookmark_tabs: Wiki

For detailed information on each module, [check our internal wiki](./docs/wiki):
- [WiFi Guide](./docs/wiki/WiFi.md) | [BLE Guide](./docs/wiki/BLE.md) | [Files & Storage](./docs/wiki/Files.md) | [IR Engine](./docs/wiki/IR.md) | [UI & Customization](./docs/wiki/UI.md)

## :heart: Sources & Credits

BruceMod is built upon the incredible work of the ESP32 and security communities. We would like to credit and thank:
- **[bmorcelli/Bruce](https://github.com/bmorcelli/bruce)**: The foundation of this firmware.
- **[AdvanceOS](https://github.com/S-V-C/AdvanceOS)**: Inspiration for the enhanced file explorer and UI elements.
- **[ESP32-Marauder](https://github.com/justcallmekoko/ESP32Marauder)**: For the core WiFi and BLE attack logic.
- **[Evil-M5Project](https://github.com/7h30th3r0n3/Evil-M5Project)**: For specialized BLE spam and security tools.
- **[Cardputer-Battery](https://github.com/InSane84/cardputer-battery)**: For the battery monitoring and status logic.
- **[Ultimate Remote](https://github.com/Zonque/Ultimate-Remote)**: Inspiration for the Advanced IR and protocol support.

## :keyboard: Discord Server

Contact us in our [Discord Server](https://discord.gg/WJ9XF9czVT)!

## :computer: List of Features

<details>
  <summary><h2>WiFi (Categorized)</h2></summary>

- [x] Connectivity (Connect, AP, Wireguard)
- [x] Attacks (Evil Portal, Bad Msg, Sleep, Responder)
- [x] Sniffers (RAW, Probe/Karma, Packet Count)
- [x] Network Tools (ARP Scan, Port Scan, TelNet/SSH, TCP)
- [x] Pass Recovery & BruceModgotchi
</details>

<details>
  <summary><h2>BLE</h2></summary>

- [X] BLE Scan & Bad BLE
- [X] Wall of Flippers / Airtags / Skimmers (Optimized)
- [X] iOS / Windows / Android / Samsung Spam
- [X] Airtag Spoofing
</details>

<details>
  <summary><h2>RF & IR</h2></summary>

- [x] RF: Scan, Replay, Custom SubGhz, Jammer, Spectrum
- [x] IR: TV-B-Gone, Capture, Custom `.ir` Files, MakeHex Engine
</details>

<details>
  <summary><h2>Others</h2></summary>

- [X] Enhanced File Manager (Recursive Copy/Paste, Icons)
- [X] USB Mass Storage + Speedometer
- [X] Audio Player (Themed)
- [X] WebUI, QRCodes, iButton, LED Control
</details>

## :construction: Disclaimer

BruceMod is a tool for cyber offensive and red team operations, distributed under the terms of the Affero General Public License (AGPL). It is intended for legal and authorized security testing purposes only. Use at your own risk.
