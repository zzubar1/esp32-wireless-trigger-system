# ESP32 ESP-NOW Receiver / Executor

Firmware for the **receiver** board in a two-ESP32 remote trigger system. This board listens for ESP-NOW commands from a companion "transmitter" ESP32 (which hosts a web control panel) and, on command, activates a light and a tone through a speaker. Sending `1` turns the light/sound on; sending `0` turns them off.

This is one half of a two-board project. See the [Transmitter / AP board](#companion-transmitter-board) section below for the other half.

## How it works

1. This board boots into WiFi **station mode** (required by ESP-NOW, even though it never joins a network) and initializes ESP-NOW.
2. It registers a receive callback (`OnDataRecv`) that fires whenever a packet arrives from a paired ESP-NOW peer.
3. The transmitter board sends a single-byte command:
   - `1` → trigger: light turns on, speaker starts playing a tone
   - `0` → end: light turns off, speaker stops
4. This board has no web server or user interface of its own. It's purely an executor that reacts to incoming ESP-NOW packets.

## Hardware needed

## Component 
-  ESP32 dev board | Any ESP32 with WiFi (tested on boards using USB Micro-B) 
-  LED or 3.3V indicator light | Connected to `GPIO 21` 
-  Small speaker | 8Ω or similar, low-power 
-  PAM8403 audio amplifier | Drives the speaker from the ESP32's PWM/DAC output 
-  Jumper wires / breadboard | Use a solid breadboard connection. Loose pins were a real source of bugs during testing 
-  USB Micro-B cable | For flashing and serial monitor 

### Wiring

- `GPIO 21` → LED / light circuit (digital HIGH/LOW, ~3.3V, ~40mA max. Don't drive a high-current load directly off this pin)
- `GPIO 25` → PAM8403 amp input → speaker (this is one of the ESP32's two built-in DAC-capable pins, used here as a PWM audio-ish output)

## Software requirements

- [Arduino IDE](https://www.arduino.cc/en/software) or PlatformIO
- ESP32 board support package installed
- Libraries used (all part of the standard ESP32 Arduino core, no extra install needed):
  - `WiFi.h`
  - `esp_now.h`
  - `esp_wifi.h`

## Setup

1. Flash this sketch to the receiver ESP32.
2. Open the Serial Monitor at **115200 baud**. On boot it prints the board's MAC address:
   ```
   MAC Address: XX:XX:XX:XX:XX:XX
   ```
3. Copy that MAC address into the `receiverMAC[]` array in the **transmitter** board's sketch.  ESP-NOW has no dynamic peer discovery, so this pairing has to be hardcoded.
4. Power on both boards. The transmitter hosts a web page; pressing its **TRIGGER** button sends `1` to this board, and **END** sends `0`.

## Known limitations / TODO

- `AUTO_OFF_DURATION` is defined but never checked. There's currently no automatic shutoff timer after a trigger; the light/sound stay on until an explicit `0` command arrives.
- The DAC-based tone is a fixed-frequency PWM square wave standing in for real audio, not an actual audio signal. The built-in DAC behaved inconsistently in testing, so a constant-frequency tone is used instead of a proper waveform.
- No encryption is used on the ESP-NOW link (matches the transmitter side). Fine for a local trusted setup, not recommended for untrusted environments.
- `loop()` is currently empty (`continue;`) since all logic runs in the ESP-NOW receive callback.

## Companion transmitter board

The transmitter ESP32:
- Hosts a WiFi access point (`WIFI_AP_STA` mode) with an SSID/password of your choosing
- Serves a simple web page with **TRIGGER** and **END** buttons
- On button press, sends the corresponding command byte to this receiver board's MAC address over ESP-NOW

Both boards need matching hardcoded MAC addresses to talk to each other.
