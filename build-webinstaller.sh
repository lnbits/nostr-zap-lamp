#!/bin/sh
PROJECT_NAME=nostrZapLamp
REPO=https://github.com/lnbits/nostr-zap-lamp/releases/download
INSTALLER_PATH=./hardware-installer/public/firmware/esp32

git clone https://github.com/lnbits/hardware-installer

cp versions.json ./hardware-installer/src/versions.json
cp installer/config.js ./hardware-installer/src/config.js
cp installer/Hero.jsx ./hardware-installer/src/Hero.jsx

sed -i "s/%title%/$PROJECT_NAME/g" ./hardware-installer/index.html

mkdir -p $INSTALLER_PATH
for version in $(jq -r '.versions[]' ./hardware-installer/src/versions.json); do
    mkdir -p $INSTALLER_PATH/$version
    wget $REPO/$version/$PROJECT_NAME.ino.bin
    mv $PROJECT_NAME.ino.bin $INSTALLER_PATH/$version
    wget $REPO/$version/$PROJECT_NAME.ino.partitions.bin
    mv $PROJECT_NAME.ino.partitions.bin $INSTALLER_PATH/$version
    wget $REPO/$version/$PROJECT_NAME.ino.bootloader.bin
    mv $PROJECT_NAME.ino.bootloader.bin $INSTALLER_PATH/$version
done
