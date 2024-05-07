# Nostr Zap Lamp

![](https://github.com/lnbits/nostr-zap-lamp/raw/main/img/lamp.jpg)

Get a visual "Zap!"" when someone zaps you or anyone on Nostr.

The Nostr Zap Lamp flashes when a relay broadcasts a Nostr event of kind 9735. You can set the lamp to flash zaps sent to a specific npub, or any npub on nostr.

The number of flashes corresponds to the number of digits in the number of sats sent. i.e. 3 sats will result in 1 flash, 152 sats will result in 3 flashes, 1001 sats will result in 4 flashes.

[Buy a pre-assembled or complete Zap Lamp kit on the LNbits shop](https://shop.lnbits.com/product/nostr-zap-lamp)

# Features

+ Show a signal when a zap is sent to one or all nostr npubs
+ Background brightness control
+ Easily configure to use your own nostr relay
+ Simple configuration to work with any npub

## Parts
+ [LED "Neon" lamp](https://www.amazon.co.uk/YIVIYAR-Lightning-Battery-Bedroom-Christmas/dp/B08K4SCVKQ)
+ An ESP32 dev board
+ USB cable
+ A momentary switch push button - For example https://www.amazon.com/Momentary-Spring-Return-Self-Return-Pushbutton-Switches/dp/B09DJY5Y5L
+ Four jumper cables
+ Block connectors
+ A case. [This repo includes files for a 3D printed case](enclosure)

## Build Instructions

+ Identify the positive and negative leads of the lamp. Tie a knot in the +ve lead for future identification
+ Cut the lamp leads to around 10cm long
+ Attach a female jumper lead to each. Solder or use block connectors to attach the leads.
+ Attach female jumper leads to the push button
+ Connect the lamp to the ESP32: GPIO13 on the ESP32 -> lamp's positive wire and GND on the ESP32 -> lamp's negative wire.
+ Connect the push button button to the ESP32 on GPIO4 and GND
+ Install the lamp, ESP32, button and lead in the printed enclosure
+ Power on the device and configure using the WiFi access point portal.

## Firmware Upload Instructions

+ Use the web installer at https://nostr-zap-lamp.lnbits.com/ to flash and configure the device
+ Alternatively, use the Arduino IDE to flash the firmware.
    - Install the ESP32 board in the Arduino IDE by adding the following URL to the board manager: https://dl.espressif.com/dl/package_esp32_index.json
    - Install the following libraries using the Arduino IDE library manager:
        - uBitcoin
        - WebSockets
        - ArduinoJson
        - base64
        - WiFiManager
        - Nostr
    - Open the nostrZapLamp.ino file in the Arduino IDE
    - Select the ESP32 Dev Module board and the correct port
    - Compile and upload the firmware
    - Use the web installer at https://nostr-zap-lamp.lnbits.com/ to configure the device
        
## 5v Relay option

Using a 5v Relay the lamp will be brighter, or you can swap out the light and connect anything to the relay that you want to turn on.

ESP32 GND <==========================> Relay "DC-"

ESP32 5V <==========================> Relay "DC+"

LED POS <===========================> Relay "DC+"

ESP32 GND <=========================> Relay "COM"

LED GND <===========================> Relay "NO"

GPIO 13 <===========================> Relay "S"

<img src="https://github.com/lnbits/nostr-zap-lamp/assets/33088785/7e265c4e-256c-49fb-a936-c3c6ccfa081b" style="width:300px">
