#!/bin/sh
INSTALLER_PATH=./hardware-installer/src/firmware/esp32
REPO=https://github.com/lnbits/bitcoinswitch/releases/download
PROJECT_NAME=nostrZapLamp

git clone https://github.com/lnbits/hardware-installer
mkdir -p $INSTALLER_PATH
cp versions.json ./hardware-installer/src/versions.json

for version in $(jq -r '.versions[]' ./hardware-installer/src/versions.json); do
    mkdir -p $INSTALLER_PATH/$version
    wget $REPO/$version/bootloader.bin
    mv bootloader.bin $INSTALLER_PATH/$version
    wget $REPO/$version/boot_app0.bin
    mv boot_app0.bin $INSTALLER_PATH/$version
    wget $REPO/$version/$PROJECT_NAME.ino.bin
    mv $PROJECT_NAME.ino.bin $INSTALLER_PATH/$version
    wget $REPO/$version/$PROJECT_NAME.ino.partitions.bin
    mv $PROJECT_NAME.ino.partitions.bin $INSTALLER_PATH/$version
done
