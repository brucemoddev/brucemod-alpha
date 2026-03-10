# WiFi Module

The WiFi module is reorganized into categories for easier navigation.

## Categories

### 1. Connectivity
- **Connect to WiFi**: Scan and connect to APs.
- **Start WiFi AP**: Start a local access point.
- **Wireguard**: Setup VPN tunneling.

### 2. Attacks
- **Standard Attacks**: Includes Beacon Spam, Deauth, etc.
- **Evil Portal**: Captive portal attack. (WebUI must be stopped first).
- **Bad Msg Attack**: Spoofed malformed management frames.
- **Sleep Attack**: Sends power-save frames to disconnect clients.
- **Responder**: LLMNR/NBT-NS poisoning.

### 3. Sniffers
- **Raw Sniffer**: Captures all 802.11 packets on a channel.
- **Probe Sniffer**: Detects device probe requests (Karma).
- **Packet Count**: Real-time stats of MGMT, DATA, and CTRL frames.

### 4. Network Tools
- **Scan Hosts**: ARP scanner for local network.
- **Port Scanner**: Scans common ports on a target IP.
- **TelNet / SSH**: Remote access clients.

### 5. Config
- **Change MAC**: Spoof your WiFi MAC address.
- **Evil WiFi Settings**: Configure endpoints and credentials access.

## Shortcuts
- **ESC**: Go back / Stop current attack.
- **Arrows/Wheel**: Navigate menu.
- **Enter/Select**: Confirm selection.
