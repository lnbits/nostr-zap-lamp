#define ESP_DRD_USE_SPIFFS true

// Include Libraries
//#include ".h"
#include <WiFi.h>
#include <SPIFFS.h>
#include <FS.h>

#include <WiFiManager.h>
#include <ArduinoJson.h>
#include "wManager.h"

// JSON configuration file
#define JSON_CONFIG_FILE "/config.json"

// Flag for saving data
bool shouldSaveConfig = false;

int buttonPin = 4; // Pin number where the button is connected

// Variables to hold data from custom textboxes
char npubHexString[80] = "";
char relayString[80] = "legend.lnbits.com/nostrclient/api/v1/relay";

bool hasInternetConnection = false;

// Define WiFiManager Object
WiFiManager wm;

void saveConfigFile()
// Save Config in JSON format
{
  Serial.println(F("Saving configuration..."));
  
  // Create a JSON document
  StaticJsonDocument<512> json;
  json["npubHexString"] = npubHexString;
  json["relayString"] = relayString;

  // Open config file
  File configFile = SPIFFS.open(JSON_CONFIG_FILE, "w");
  if (!configFile)
  {
    // Error, file did not open
    Serial.println("failed to open config file for writing");
  }

  // Serialize JSON data to write to file
  serializeJsonPretty(json, Serial);
  if (serializeJson(json, configFile) == 0)
  {
    // Error writing file
    Serial.println(F("Failed to write to file"));
  }
  // Close file
  configFile.close();
}

bool loadConfigFile()
// Load existing configuration file
{
  // Uncomment if we need to format filesystem
  // SPIFFS.format();

  // Read configuration from FS json
  Serial.println("Mounting File System...");

  // May need to make it begin(true) first time you are using SPIFFS
  if (SPIFFS.begin(false) || SPIFFS.begin(true))
  {
    Serial.println("mounted file system");
    if (SPIFFS.exists(JSON_CONFIG_FILE))
    {
      // The file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open(JSON_CONFIG_FILE, "r");
      if (configFile)
      {
        Serial.println("Opened configuration file");
        StaticJsonDocument<512> json;
        DeserializationError error = deserializeJson(json, configFile);
        configFile.close();
        serializeJsonPretty(json, Serial);
        if (!error)
        {
          Serial.println("Parsing JSON");

          strcpy(npubHexString, json["npubHexString"]);
          strcpy(relayString, json["relayString"]);
          return true;
        }
        else
        {
          // Error loading JSON data
          Serial.println("Failed to load json config");
        }
      }
    }
  }
  else
  {
    // Error mounting file system
    Serial.println("Failed to mount FS");
  }

  return false;
}


void saveConfigCallback()
// Callback notifying us of the need to save configuration
{
  Serial.println("Should save config");
  shouldSaveConfig = true;
  //wm.setConfigPortalBlocking(false);
}

void configModeCallback(WiFiManager *myWiFiManager)
// Called when config mode launched
{
  Serial.println("Entered Configuration Mode");

  Serial.print("Config SSID: ");
  Serial.println(myWiFiManager->getConfigPortalSSID());

  Serial.print("Config IP Address: ");
  Serial.println(WiFi.softAPIP());
}

void init_WifiManager()
{
  Serial.begin(115200);
  //Serial.setTxTimeoutMs(10);

  //Init pin 15 to eneble 5V external power (LilyGo bug)
  pinMode(PIN_ENABLE5V, OUTPUT);
  digitalWrite(PIN_ENABLE5V, HIGH);

  // Change to true when testing to force configuration every time we run
  bool forceConfig = false;
  // Check if button is pressed to enter configMode with actual configuration
  if(digitalRead(buttonPin) == LOW){
    Serial.println(F("Forcing config mode"));
    forceConfig = true;
    // clear any SPIFFs storage
    wm.resetSettings();
    delay(1000);
    wm.setBreakAfterConfig(true); //Set to detect config edition and save
 }
  bool spiffsSetup = loadConfigFile();
  if (!spiffsSetup)
  {
    Serial.println(F("Forcing config mode as there is no saved config"));
    forceConfig = true;    
  }

  // Explicitly set WiFi mode
  WiFi.mode(WIFI_STA);

  // Reset settings (only for development)
  //wm.resetSettings();

  //Set dark theme
  //wm.setClass("invert"); // dark theme
  
  // Set config save notify callback
  wm.setSaveConfigCallback(saveConfigCallback);

  wm.setHostname("NostrZapLamp");
  wm.setTitle("NostrZapLamp");

  // Set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wm.setAPCallback(configModeCallback);

  //Advanced settings
  wm.setConfigPortalBlocking(false); //Hacemos que el portal no bloquee el firmware
  wm.setConnectTimeout(50); // how long to try to connect for before continuing
  //wm.setConfigPortalTimeout(30); // auto close configportal after n seconds
  // wm.setCaptivePortalEnable(false); // disable captive portal redirection
  // wm.setAPClientCheck(true); // avoid timeout if client connected to softap
  //wm.setTimeout(120);
  //wm.setConfigPortalTimeout(120); //seconds
  
  // Custom elements

  // Text box (String) - 80 characters maximum
  WiFiManagerParameter npub_text_box("npubhex", "npub in hex format", npubHexString, 80);

  WiFiManagerParameter relay_text_box_num("relay", "Prefered relay", relayString, 80);

  // Add all defined parameters
  wm.addParameter(&npub_text_box);
  wm.addParameter(&relay_text_box_num);

  Serial.println("All Done: ");
  if (forceConfig)
    // Run if we need a configuration
  {
    //No configuramos timeout al modulo
    wm.setConfigPortalBlocking(true); //Hacemos que el portal SI bloquee el firmware
    // tft.pushImage(0, 0, setupModeWidth, setupModeHeight, setupModeScreen);
    if (!wm.startConfigPortal("ZapLampAP","ZipZapZop"))
    {
      Serial.println("failed to connect and hit timeout");
      //Could be break forced after edditing, so save new config
      strncpy(npubHexString, npub_text_box.getValue(), sizeof(npubHexString));
      strncpy(relayString, relay_text_box_num.getValue(), sizeof(relayString));
      
      saveConfigFile();
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.restart();
      delay(5000);
    }
  }
  else
  {
    //Tratamos de conectar con la configuración inicial ya almacenada
    wm.setCaptivePortalEnable(false); // disable captive portal redirection
    if (!wm.autoConnect("ZapLampAP","ZipZapZop"))
    {
      Serial.println("Failed to connect and hit timeout");
      //delay(3000);
      // if we still have not connected restart and try all over again
      //ESP.restart();
      //delay(5000);
    }
  }

  //Conectado a la red Wifi
  if(WiFi.status() == WL_CONNECTED){
    //tft.pushImage(0, 0, MinerWidth, MinerHeight, MinerScreen);
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    hasInternetConnection = true;

    // Lets deal with the user config values
  
    // Copy the string value
    strncpy(npubHexString, npub_text_box.getValue(), sizeof(npubHexString));
    Serial.print("npubhex: ");
    Serial.println(npubHexString);
  
    //Convert the number value
    strncpy(relayString, relay_text_box_num.getValue(), sizeof(relayString));
    Serial.print("relay: ");
    Serial.println(relayString);

  }
  
  // Save the custom parameters to FS
  if (shouldSaveConfig)
  {
    saveConfigFile();
  }
}

void reset_configurations() {
  Serial.println("Erasing Config, restarting");
  wm.resetSettings();
  SPIFFS.remove(JSON_CONFIG_FILE); //Borramos fichero
  ESP.restart();
}


//----------------- MAIN PROCESS WIFI MANAGER --------------
int oldStatus = 0;

void wifiManagerProcess() {
  
  wm.process(); // avoid delays() in loop when non-blocking and other long running code
  
  int newStatus = WiFi.status();
  if (newStatus != oldStatus) {
    if (newStatus == WL_CONNECTED) {
      Serial.println("CONNECTED - Current ip: " + WiFi.localIP().toString());
    } else {
      Serial.print("[Error] - current status: ");
      Serial.println(newStatus);
    }
    oldStatus = newStatus;
  }
}


