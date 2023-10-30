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
    name: "ssid",
    value: "",
    label: "WiFi SSID",
    type: "text",
  },
  {
    name: "password",
    value: "",
    label: "WiFi Password",
    type: "text",
  },
  {
    name: "relay",
    value: "",
    label: "Nostr Relay URL",
    type: "text",
  },
  {
    name: "pubkey",
    value: "",
    label: "Nostr Public Key in hex",
    type: "text",
  },
];
