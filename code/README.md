# Nostr Zap Lamp

![](img/lamp.jpg)

Get a visual Zap! when someone zaps your or anyone on Nostr. 

The Nostr Zap Lamp flashes when a relay broadcasts a Nostr event of kind 9735. You can set the lamp to flash zaps sent to a specific npub, or any npub on nostr.

The number of flashes corresponds to the number of digits in the number of sats sent. i.e. 3 sats will result in 1 flash, 152 sats will result in 3 flashes, 1001 sats will result in 4 flashes.

[Buy a pre-assembled or complete Zap Lamp kit on the LNbits shop](https://shop.lnbits.com/product/nostr-zap-lamp)

# Features

+ Show a signal when a zap is sent to one or all nostr npubs
+ Background brightness control
+ Easily configure to use your own nostr relay
+ Easy configuration to work with any npub

## Parts
+ [LED "Neon" lamp](https://www.amazon.co.uk/YIVIYAR-Lightning-Battery-Bedroom-Christmas/dp/B08K4SCVKQ)
+ An ESP32 dev board
+ USB cable
+ A momentary switch push button - For example https://www.amazon.com/Momentary-Spring-Return-Self-Return-Pushbutton-Switches/dp/B09DJY5Y5L 
+ Four jumper cables
+ Block connectors
+ A case. [This repo includes files for a 3D printed case](enclosure)

## Usage instructions
1. Hold button and power on to launch the lamp into access point mode. a solid lamp light inidicates the device is in AP mode.
1. Using a smart phone or computer, connect to the ZapLamp's Wifi AP named "ZapLampAP" with the password "ZipZapZop"
1. When the AP portal opens, select "Configure WiFi"
1. Select your WiFi router and enter it's password into the password field.
1. Optional: Enter the _hex_ value of your npub to watch for Zaps. Use [this tool](https://slowli.github.io/bech32-buffer/) to convert bech32 npub to hex. If you leave this field blank, the lamp will react to all zaps on the relay.
1. Enter a prefered relay if desired and press save

## Build Instructions

+ Identify the positive and negative leads of the lamp. Tie a knot in the +ve lead for future identification
+ Cut the lamp leads to around 10cm long
+ Attach a female jumper lead to each. Solder or use block connectors to attach the leads.
+ Attach female jumper leads to the push button
+ Connect the lamp to the ESP32: GPIO13 on the ESP32 -> lamp's positive wire and GND on the ESP32 -> lamp's negative wire.
+ Connect the push button button to the ESP32 on GPIO4 and GND
+ Install the lamp, ESP32, button and lead in the printed enclosure
+ Power on the device and configure using the WiFi access point portal.
