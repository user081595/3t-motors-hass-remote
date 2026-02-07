# 3T-Motors Shutter Control

This project aims to control your 3T-Motors shutters via HomeAssistant (MQTT).
It is required that your shutter is already connected to a remote, so we are able to record commands that are sent to your shutters.

## Tested Hardware:

   - 3T-Motors
      - WSE1 (Remote)
      - WSE2 (Remote)
      - SW60 3T45-10R (Shutter)
   - Dooya?
   - Rohrmotor24?
   - Rollladenmotor24?

## Required Hardware

To create this roject we just need an ESP32 and a CC1101 module.
https://de.aliexpress.com/item/1005002010800512.html
https://de.aliexpress.com/item/1005005200440286.html

Connect Pins from CC1101 to ESP32:
VCC = 3V
SCK_PIN = 18
MISO_PIN (SO) = 19
MOSI_PIN (SI) = 23
SS_PIN (CSN) = 5
GMO (GDO0) = 32
GD2 (GDO2) = 33

## Quick Start

Open this project in VSCode.
Make sure the PlatformIO extension is installed.
In main.cpp setup your wifiConfig and mqttConfig.
You can ignore your shutters config for now.
Connect your ESP via USB and compile/upload the firmware.

In HomeAssistant under "Integrations" -> "MQTT" you should see now "3T-Motors Remote".
When you click on it, you should see the device "Record Shutter Signal".
The "Stop" device will start/stop recording. Press it once and under sensors you should see "Recording".
Now press the "Stop" button on your real 3T-Motors remote. Afterwards press "Stop" in Home Assistant again.
The sensor that showed "Recording" before now should show a long string.
This is the shutter id you want to add in your config.
You can press "Open" or "Close" buttons to check if the remote is working as expected.
When the shutters are working as expected, you can add the id to the config.
The first value is a unique id, the second value is the name and the third value is the shutter id.
You can now repeat to record all devices and copy all ids to the config.


```
.shutters = {
    {"shutter_living_room", "Living room", "45232323333245453345452323452345454523232345454545454545452345234"},
},
```

When all shutters are added, compile and flash the firmware again.
You can now disable the "Record Shutter Signal" and the "Last Recorded Signal" in HomeAssistant.

## Disclaimer

This is a reverse-engineered implementation based on observed RF behavior.
No official protocol documentation from the manufacturer is used.

Use at your own risk.


## Used Libraries

This project includes third-party components:

1) SmartRC-CC1101-Driver-Lib (slightly modified)
   Copyright (c) 2010 Michael (ELECHOUSE)
   Modifications Copyright (c) 2018–2020 Little Satan
   License: Custom / permissive license
   You may freely use, edit, or distribute with reference to the source.
   Original repository: https://github.com/LSatan/SmartRC-CC1101-Driver-Lib

2) arduino-home-assistant
   Copyright (c) Dawid Chyrzynski
   License: GNU Affero General Public License v3.0
   Repository: https://github.com/dawidchyrzynski/arduino-home-assistant