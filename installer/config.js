export const addressesAndFiles = [
  {
    address: "0x1000",
    fileName: "nostrZapLamp.ino.bootloader.bin",
  },
  {
    address: "0x8000",
    fileName: "nostrZapLamp.ino.partitions.bin",
  },
  {
    address: "0xE000",
    fileName: "boot_app0.bin",
  },
  {
    address: "0x10000",
    fileName: "nostrZapLamp.ino.bin",
  },
];

export const configPath = "elements.json";
export const elements = [
  {
    name: "config_ssid",
    value: "",
    label: "WiFi SSID",
    type: "text",
  },
  {
    name: "config_wifi_password",
    value: "",
    label: "WiFi Password",
    type: "text",
  },
  {
    name: "config_relay",
    value: "nos.lol",
    label: "Nostr Relay URL",
    type: "text",
  },
  {
    name: "config_pubkey",
    value: "",
    label: "The Public Key in Hex to Watch for Zaps",
    type: "text",
  },
];
