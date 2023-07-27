#include <Arduino.h>
#include "WiFiClientSecure.h"
#include "time.h"
#include <NostrEvent.h>
#include <NostrRelayManager.h>
#include <NostrRequestOptions.h>
#include <Wire.h>
#include "Bitcoin.h"
#include "Hash.h"
#include <esp_random.h>
#include "QRCode.h"
#include <math.h>
#include <SPIFFS.h>

#include <ArduinoJson.h>
#include <AutoConnect.h>
#include <WebServer.h>
#include "access_point.h"

#define PARAM_FILE "/elements.json"

int triggerAp = false;

int ledPin = 13; // Pin number where the LED is connected
int buttonPin = 4; // Pin number where the button is connected
int minFlashDelay = 100; // Minimum delay between flashes (in milliseconds)
int maxFlashDelay = 5000; // Maximum delay between flashes (in milliseconds)
int lightBrightness = 50; // The brightness of the LED (0-255)

NostrEvent nostr;
NostrRelayManager nostrRelayManager;
NostrQueueProcessor nostrQueue;

String serialisedEventRequest;

NostrRequestOptions* eventRequestOptions;

bool hasSentEvent = false;

WebServer server;
AutoConnect portal(server);
AutoConnectConfig config;
AutoConnectAux elementsAux;
AutoConnectAux saveAux;
String apPassword = "ToTheMoon1"; //default WiFi AP password
String npubToWatch = "";
String relay = "relay.damus.io";

fs::SPIFFSFS &FlashFS = SPIFFS;
#define FORMAT_ON_FAIL true

// define funcs
void configureAccessPoint();
void initWiFi();
bool whileCP(void);
void changeBrightness();
void signalWithLightning(int numFlashes, int duration);
void flashLightning(int zapAmountSats);
void doLightningFlash(int numberOfFlashes);
void fadeOutFlash(int intensity);
void initLamp();
unsigned long getUnixTimestamp();
void zapReceiptEvent(const std::string& key, const char* payload);
void okEvent(const std::string& key, const char* payload);
void nip01Event(const std::string& key, const char* payload);
uint8_t getRandomNum(uint8_t min, uint8_t max);
void relayConnectedEvent(const std::string& key, const std::string& message);
void loadSettings();
int64_t getAmountInSatoshis(const String &input);
String getBolt11InvoiceFromEvent(String jsonStr);
void createZapEventRequest();
void connectToNostrRelays();


/**
 * @brief Create a Zap Event Request object
 * 
 */
void createZapEventRequest() {
  // Create the REQ
  eventRequestOptions = new NostrRequestOptions();
  // Populate kinds
  int kinds[] = {9735};
  eventRequestOptions->kinds = kinds;
  eventRequestOptions->kinds_count = sizeof(kinds) / sizeof(kinds[0]);

  // // Populate #p
  if(npubToWatch != "") {
    String* pubkeys = new String[1];  // Allocate memory dynamically
    pubkeys[0] = npubToWatch;
    eventRequestOptions->p = pubkeys;
    eventRequestOptions->p_count = 1;
  }

  eventRequestOptions->limit = 0;

  // We store this here for sending this request again if a socket reconnects
  serialisedEventRequest = "[\"REQ\", \"" + nostrRelayManager.getNewSubscriptionId() + "\"," + eventRequestOptions->toJson() + "]";

  delete eventRequestOptions;
}

/**
 * @brief Connect to the Nostr relays
 * 
 */
void connectToNostrRelays() {
  Serial.println("Requesting Zap notifications");

      const char *const relays[] = {
          relay.c_str(),
          "nostr.wine",
      };
      int relayCount = sizeof(relays) / sizeof(relays[0]);
      
      nostr.setLogging(false);
      nostrRelayManager.setRelays(relays, relayCount);
      nostrRelayManager.setMinRelaysAndTimeout(1,10000);

      // Set some event specific callbacks here
      nostrRelayManager.setEventCallback("ok", okEvent);
      nostrRelayManager.setEventCallback("connected", relayConnectedEvent);
      nostrRelayManager.setEventCallback(9735, zapReceiptEvent);

      nostrRelayManager.connect();
}

// Define the WiFi event callback function
void WiFiEvent(WiFiEvent_t event) {
  switch(event) {
    case SYSTEM_EVENT_STA_GOT_IP:
      Serial.println("Connected to WiFi and got an IP");
      
      connectToNostrRelays();      
      
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      Serial.println("Disconnected from WiFi");
      // WiFi.begin(ssid, password); // Try to reconnect after getting disconnected
      break;
  }
}

void initWiFi() {
  int buttonState = digitalRead(buttonPin);
  Serial.println(F("button pin value is"));
  Serial.println(buttonState);
  if (buttonState != HIGH) {
      Serial.println("Launch portal");
      triggerAp = true;
  } else {
    Serial.println("Button state is low. Dont auto-launch portal.");
  }

  WiFi.onEvent(WiFiEvent);

  configureAccessPoint();
    
  WiFi.mode(WIFI_STA);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
  }

  signalWithLightning(3,100);
}

/**
 * @brief While captive portal callback
 * 
 * @return true 
 * @return false 
 */
bool whileCP(void) {
  bool rc;
  // light the led full brightness
  analogWrite(ledPin, 255);
  rc = true;
  return rc;
}

/**
 * @brief Configure the WiFi AP
 * 
 */
void configureAccessPoint() {
  // handle access point traffic
  server.on("/", []() {
    String content = "<h1>Nostr Zap Lamp</h1>";
    content += AUTOCONNECT_LINK(COG_24);
    server.send(200, "text/html", content);
  });

  elementsAux.load(FPSTR(PAGE_ELEMENTS));

  saveAux.load(FPSTR(PAGE_SAVE));
  saveAux.on([](AutoConnectAux &aux, PageArgument &arg) {
    aux["caption"].value = PARAM_FILE;
    File param = FlashFS.open(PARAM_FILE, "w");

    if (param)
    {
        // save as a loadable set for parameters.
        elementsAux.saveElement(param, {"ap_password", "npub_hex_to_watch", "relay"});
        param.close();

        // read the saved elements again to display.
        param = FlashFS.open(PARAM_FILE, "r");
        aux["echo"].value = param.readString();
        param.close();
    }
    else
    {
        aux["echo"].value = "Filesystem failed to open.";
    }
    return String();
  });

  elementsAux.on([](AutoConnectAux &aux, PageArgument &arg) {
    File param = FlashFS.open(PARAM_FILE, "r");
    if (param)
    {
      aux.loadElement(param, {"ap_password", "npub_hex_to_watch", "relay"});
      param.close();
    }

    if (portal.where() == "/settings")
    {
      File param = FlashFS.open(PARAM_FILE, "r");
      if (param)
      {
        aux.loadElement(param, {"ap_password", "npub_hex_to_watch", "relay"});
        param.close();
      }
    }
    return String();
  });

  config.autoReconnect = true;
  config.reconnectInterval = 1; // 30s
  config.beginTimeout = 30000UL;

  Serial.println("Trigger AP: " + String(triggerAp));
  config.immediateStart = triggerAp;
  config.hostName = "ZapLamp";
  config.apid = "ZapLamp-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  config.apip = IPAddress(21, 1, 16, 19);      // Sets SoftAP IP address
  config.gateway = IPAddress(21, 1, 16, 19);     // Sets WLAN router IP address
  config.psk = apPassword;
  config.menuItems = AC_MENUITEM_CONFIGNEW | AC_MENUITEM_OPENSSIDS | AC_MENUITEM_RESET;
  config.title = "Nostr Zap Lamp";
  config.portalTimeout = 120000;

  portal.whileCaptivePortal(whileCP);

  portal.join({elementsAux, saveAux});
  portal.config(config);

    // Establish a connection with an autoReconnect option.
  if (portal.begin()) {
    Serial.println("WiFi connected: " + WiFi.localIP().toString());
  }
}

void loadSettings() {
    // get the saved details and store in global variables
  File paramFile = FlashFS.open(PARAM_FILE, "r");
  if (paramFile)
  {
    StaticJsonDocument<1000> doc;
    DeserializationError error = deserializeJson(doc, paramFile.readString());

    const JsonObject passRoot = doc[0];
    const char *apPasswordChar = passRoot["value"];
    const char *apNameChar = passRoot["name"];

    if (String(apPasswordChar) != "" && String(apNameChar) == "ap_password")
    {
      apPassword = apPasswordChar;
    }

    const JsonObject npubToWatchDoc = doc[1];
    const char *npubToWatchChar = npubToWatchDoc["value"];
    if (String(npubToWatchChar) != "") {
        npubToWatch = String(npubToWatchChar);
    }
    Serial.println("npub to watch: " + npubToWatch);

    const JsonObject relayDoc = doc[2];
    const char *relayChar = relayDoc["value"];
    if (String(relayChar) != "") {
        relay = String(relayChar);
    }
    Serial.println("relay: " + relay);

  }

  paramFile.close();
}

bool adjustLightingBrightnessUp = true;
/**
 * @brief change lamp brightness
 * 
 */
void changeBrightness() {
  // use lastLightingAdjustmentWasUp and max value of 255 to decide whether to adjust up or down
  if (adjustLightingBrightnessUp) {
    lightBrightness = lightBrightness + 5;
    if (lightBrightness >= 255) {
      lightBrightness = 255;
      adjustLightingBrightnessUp = false;
      delay(500); // pause to let the user take their finger off the button
    }
  } else {
    lightBrightness = lightBrightness - 5;
    if (lightBrightness <= 0) {
      lightBrightness = 0;
      adjustLightingBrightnessUp = true;
      delay(500); // pause to let the user take their finger off the button
    }
  }
  
  // write to spiffs
  File file = SPIFFS.open("/brightness.txt", FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file for writing");
    return;
  }
  file.println(lightBrightness);
  file.close();

  analogWrite(ledPin, lightBrightness);
  // delay(250);
}

void fadeOutFlash(int intensity) {
  analogWrite(ledPin, intensity); // set the LED to the desired intensity

  delay(50); // delay to give the LED time to light up

  // fast fade-out
  for (int i = intensity; i >= 0; i--) {
    analogWrite(ledPin, i); // set the LED brightness
    delay(1);  // wait for a moment
  }

  delay(50);
}

void doLightningFlash(int numberOfFlashes) {

  fadeOutFlash(5);
  fadeOutFlash(10);
  fadeOutFlash(15);

  delay(100);

  for(int flash = 1; flash <= numberOfFlashes; flash++) {
    // turn the LED on
    digitalWrite(ledPin, HIGH);

    // wait for the specified time, longer for the first flash and shorter for subsequent flashes
    int flashDuration = 250 / flash * random(1,5);
    delay(flashDuration / 2);

    // fast fade-out
    for (int i = 255; i >= lightBrightness; i = i - 2) {
      analogWrite(ledPin, i);  // set the LED brightness
      delay(1);  // wait for a moment
    }
  }

  delay(50);

  // fadeOutFlash(15);
  // fadeOutFlash(5);

  delay(50);

  // set led to brightness
  analogWrite(ledPin, lightBrightness);
}

void flashLightning(int zapAmountSats) {
  int flashCount = 1;
  // set flash count length of the number in the zap amount
  if (zapAmountSats > 0) {
    flashCount = floor(log10(zapAmountSats)) + 1;
  }
  Serial.println("Zap amount: " + String(zapAmountSats) + " sats");
  Serial.println("Flashing " + String(flashCount) + " times");

  doLightningFlash(flashCount);

}

/**
 * @brief Flash the LED a random number of times with a random delay between flashes
 * 
 * @param numFlashes 
 */
void signalWithLightning(int numFlashes, int duration = 500) {
  for (int i = 0; i < numFlashes; i++) {
    digitalWrite(ledPin, HIGH);
    delay(duration);

    digitalWrite(ledPin, LOW);
    delay(duration);
  }
}


unsigned long getUnixTimestamp() {
  time_t now;
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return 0;
  } else {
    Serial.println("Got timestamp of " + String(now));
  }
  time(&now);
  return now;
}

String lastPayload = "";

void relayConnectedEvent(const std::string& key, const std::string& message) {
  Serial.println("Relay connected: ");
  
  Serial.print(F("Requesting events:"));
  Serial.println(serialisedEventRequest);

  nostrRelayManager.broadcastEvent(serialisedEventRequest);
}

void okEvent(const std::string& key, const char* payload) {
    if(lastPayload != payload) { // Prevent duplicate events from multiple relays triggering the same logic
      lastPayload = payload;
      Serial.println("payload is: ");
      Serial.println(payload);
    }
}

void nip01Event(const std::string& key, const char* payload) {
    if(lastPayload != payload) { // Prevent duplicate events from multiple relays triggering the same logic
      lastPayload = payload;
      // We can convert the payload to a StaticJsonDocument here and get the content
      StaticJsonDocument<1024> eventJson;
      deserializeJson(eventJson, payload);
      String pubkey = eventJson[2]["pubkey"].as<String>();
      String content = eventJson[2]["content"].as<String>();
      Serial.println(pubkey + ": " + content);
    }
}

String getBolt11InvoiceFromEvent(String jsonStr) {
  // Remove all JSON formatting characters
  String str = jsonStr.substring(1, jsonStr.length()-1); // remove the first and last square brackets
  str.replace("\\", ""); // remove all backslashes

  // Search for the "bolt11" substring
  int index = str.indexOf("bolt11");

  // Extract the value associated with "bolt11"
  String bolt11 = "";
  if (index != -1) {
    int start = index + 9; // the value of "bolt11" starts 9 characters after the substring index
    int end = start; // initialize the end index
    while (str.charAt(end) != '\"') {
      end++; // increment the end index until the closing double-quote is found
    }
    bolt11 = str.substring(start, end); // extract the value of "bolt11"
  }
  return bolt11;
}

/**
 * @brief Get the Amount In Satoshis from a lightning bol11 invoice
 * 
 * @param input 
 * @return int64_t 
 */
int64_t getAmountInSatoshis(const String &input) {
    int64_t number = -1;
    char multiplier = ' ';

    for (unsigned int i = 0; i < input.length(); ++i) {
        if (isdigit(input[i])) {
            number = 0;
            while (isdigit(input[i])) {
                number = number * 10 + (input[i] - '0');
                ++i;
            }
            for (unsigned int j = i; j < input.length(); ++j) {
                if (isalpha(input[j])) {
                    multiplier = input[j];
                    break;
                }
            }
            break;
        }
    }

    if (number == -1 || multiplier == ' ') {
        return -1;
    }

    int64_t satoshis = number;

    switch (multiplier) {
        case 'm':
            satoshis *= 100000; // 0.001 * 100,000,000
            break;
        case 'u':
            satoshis *= 100; // 0.000001 * 100,000,000
            break;
        case 'n':
            satoshis /= 10; // 0.000000001 * 100,000,000
            break;
        case 'p':
            satoshis /= 10000; // 0.000000000001 * 100,000,000
            break;
        default:
            return -1;
    }

    return satoshis;
}


uint8_t getRandomNum(uint8_t min, uint8_t max) {
  uint8_t rand  = (esp_random() % (max - min + 1)) + min;
  Serial.println("Random number: " + String(rand));
  return rand;
}

void zapReceiptEvent(const std::string& key, const char* payload) {
  Serial.println("Zap receipt event");
    if(lastPayload != payload) { // Prevent duplicate events from multiple relays triggering the same logic, as we are using multiple relays, this is likely to happen
      lastPayload = payload;
      String bolt11 = getBolt11InvoiceFromEvent(payload);
      Serial.println("BOLT11: " + bolt11);
      uint64_t amountInSatoshis = getAmountInSatoshis(bolt11);
      Serial.println("Zapped! " + String(amountInSatoshis));
      flashLightning(amountInSatoshis);
    }
}

void initLamp() {
  // Set the LED pin as OUTPUT
  pinMode(ledPin, OUTPUT);

  // get brightness value from spiffs brightness.txt
  File file = SPIFFS.open("/brightness.txt");
  if(!file){
    Serial.println("Failed to open file for reading");
    return;
  }
  String brightnessStr = file.readStringUntil('\n');
  file.close();
  lightBrightness = brightnessStr.toInt();
}

void setup() {
  Serial.begin(115200);

  randomSeed(analogRead(0)); // Seed the random number generator

  FlashFS.begin(FORMAT_ON_FAIL);
  // init spiffs
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  initLamp();

  delay(2000);
  signalWithLightning(2,250);

  // Set the button pin as INPUT
  pinMode(buttonPin, INPUT_PULLUP);

  initWiFi();
  
  loadSettings();

  // Set the LED to the desired intensity
  analogWrite(ledPin, lightBrightness);

  createZapEventRequest();
}

void loop() {
  nostrRelayManager.loop();

  // watch for button press and call changeBrightness
  if (digitalRead(buttonPin) == LOW) {
    changeBrightness();
  }
}