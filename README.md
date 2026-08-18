# ESP32-MultiMac-Cloner-Toolkit

A simple ESP32 project with a built-in web portal to scan Wi-Fi networks, connect to open SSIDs, and sequentially clone/rotate selected MAC addresses with an optional infinite loop.

## Features
- Web UI hosted directly on the ESP32 (AP Mode)
- Multi-MAC rotation with checkboxes and custom MAC input
- Wi-Fi scanner & quick-connect for open networks
- Optional infinite loop mode with customizable hold time per MAC
- Custom hostname handling

## Setup & Flashing
1. Open the `.ino` file in the Arduino IDE.
2. Make sure the ESP32 board package is installed.
3. Select your board (e.g., **ESP32 Dev Module**) and flash the sketch.
4. Set the Serial Monitor to `115200` baud.

## How to Use
1. Connect to the ESP32's Wi-Fi access point:
   - **SSID:** `ESP32_Toolkit`
   - **Password:** `12345678`
2. Open your browser and go to `http://192.168.4.1`.
3. Enter your target SSID, password (if any), select your MAC addresses, and start the sequence.
