# ESP32 MultiMac Cloner Toolkit

An advanced embedded networking toolkit designed for the ESP32, featuring a dark-mode web dashboard, persistent flash configuration storage, state machine automation, and dynamic MAC address rotation.

---

## Features

- **Embedded Web Dashboard:** Responsive dark-mode administrative interface served directly from the ESP32 Access Point (`192.168.4.1`) for mobile and desktop control.
- **State Machine Architecture:** Robust operational management across four distinct states (`IDLE`, `CLONING`, `COOLING`, and `ENERGY_SAVER`).
- **Persistent Flash Storage:** Securely saves and loads target SSIDs and passwords across reboots using the Arduino `Preferences` library.
- **MAC Rotation & Hostname Spoofing:** Selective hardware address spoofing combined with dynamic client hostname switching (e.g., simulating set-top boxes) to test association table handling.
- **Thermal & Energy Management:** Built-in active session limits (1 hour) triggering automated cool-down periods, alongside low-power range checking when targets go offline.

---

## Hardware & Environment Requirements

- **Microcontroller:** ESP32 Development Board (NodeMCU-32s, ESP32-WROOM-32, etc.)
- **IDE:** Arduino IDE or PlatformIO
- **Dependencies:** 
  - `WiFi` (Built-in)
  - `WebServer` (Built-in)
  - `esp_wifi.h` (Built-in)
  - `Preferences` (Built-in)

---

## Getting Started

1. **Clone the Repository:**
   ```bash
   git clone [https://github.com/ASIF58/ESP32-MultiMac-Cloner-Toolkit.git](https://github.com/ASIF58/ESP32-MultiMac-Cloner-Toolkit.git)
