///////////////////////////////////////////////////////////////////////////////////
//         Change these variables directly in the code or use the config         //
//  form in the web-installer https://lnbits.github.io/nostr-zap-lamp/installer/  //
///////////////////////////////////////////////////////////////////////////////////

String version = "0.0.1";

String config_ssid = "null"; // 'String config_ssid = "config_ssid";' / 'String config_ssid = "null";'
String config_wifi_password = "null"; // 'String config_wifi_password = "password";' / 'String config_wifi_password = "null";'
String config_pubkey = "null";
String config_relay = "null";

///////////////////////////////////////////////////////////////////////////////////
//                                 END of variables                              //
///////////////////////////////////////////////////////////////////////////////////

#include <WiFi.h>
#include "time.h"
#include <NostrEvent.h>
#include <NostrRelayManager.h>
#include <NostrRequestOptions.h>
#include <Wire.h>
#include "Bitcoin.h"
#include "Hash.h"
#include <esp_random.h>
#include <math.h>
#include <SPIFFS.h>
#include <vector>
#include <ArduinoJson.h>

// freertos
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BUZZER_PIN 2      // Connect the piezo buzzer to this GPIO pin.
#define CLICK_DURATION 20 // Duration in milliseconds.

struct KeyValue {
    String key;
    String value;
};

int buttonPin = 4;
int portalPin = 15;
int triggerAp = false;

bool lastInternetConnectionState = true;

int socketDisconnectedCount = 0;
int ledPin = 13; // Pin number where the LED is connected
extern int buttonPin; // Pin number where the button is connected
int minFlashDelay = 100; // Minimum delay between flashes (in milliseconds)
int maxFlashDelay = 5000; // Maximum delay between flashes (in milliseconds)
int lightBrightness = 50; // The brightness of the LED (0-255)

bool forceConfig = false;

SemaphoreHandle_t zapMutex;

// create a vector for storing zap amount for the flash queue
std::vector<int> zapAmountsFlashQueue;

NostrEvent nostr;
NostrRelayManager nostrRelayManager;
NostrQueueProcessor nostrQueue;

String serialisedEventRequest;

bool hasInternetConnection = false;

NostrRequestOptions* eventRequestOptions;

bool hasSentEvent = false;

bool isBuzzerEnabled = true;

fs::SPIFFSFS &FlashFS = SPIFFS;
#define FORMAT_ON_FAIL true
#define PARAM_FILE "/elements.json"


// define funcs
void click(int period);
void configureAccessPoint();
void initWiFi();
bool whileCP(void);
void changeBrightness();
void signalWithLightning(int numFlashes, int duration);
void flashLightning(int zapAmountSats);
void doLightningFlash(int numberOfFlashes);
void initLamp();
unsigned long getUnixTimestamp();
void zapReceiptEvent(const std::string& key, const char* payload);
void okEvent(const std::string& key, const char* payload);
void relayConnectedEvent(const std::string& key, const std::string& message);
void relayDisonnectedEvent(const std::string& key, const std::string& message);
uint16_t getRandomNum(uint16_t min, uint16_t max);
void loadSettings();
int64_t getAmountInSatoshis(const String &input);
String getBolt11InvoiceFromEvent(String jsonStr);
void createZapEventRequest();
void connectToNostrRelays();

#define BUTTON_PIN 0 // change this to the pin your button is connected to
#define DOUBLE_TAP_DELAY 250 // delay for double tap in milliseconds

volatile unsigned long lastButtonPress = 0;
volatile bool doubleTapDetected = false;

void IRAM_ATTR handleButtonInterrupt() {
  unsigned long now = millis();
  if (now - lastButtonPress < DOUBLE_TAP_DELAY) {
    doubleTapDetected = true;
  }
  lastButtonPress = now;
}

//free rtos task for lamp control
void lampControlTask(void *pvParameters) {
  Serial.println("Starting lamp control task");
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonInterrupt, FALLING);

  for(;;) {
    if(!lastInternetConnectionState) {
      // slow fade pulse of LED
      for (int i = 100; i < 255; i++) {
        analogWrite(ledPin, i); // set the LED to the desired intensity
        delay(10);  // wait for a moment
      }
      // now fade out
      for (int i = 255; i >= 100; i--) {
        analogWrite(ledPin, i); // set the LED bright ness
        delay(10);  // wait for a moment
      }
    }

    // detect double tap on button
    if (doubleTapDetected) {
      Serial.println("Double tap detected. REstarting");
      // restart device
      ESP.restart();
    }

    // watch for button press and call changeBrightness
    if (digitalRead(buttonPin) == LOW) {
      Serial.println("Button pressed. Changing brightness");
      changeBrightness();
    } else {

      // watch for lamp state and do as needed
      if (zapAmountsFlashQueue.size() > 0) {
        //  get size of queue and serial print all elements in queue
        Serial.println("There are " + String(zapAmountsFlashQueue.size()) + " zap(s) in the queue");
        xSemaphoreTake(zapMutex, portMAX_DELAY);
        int zapAmount = zapAmountsFlashQueue[0];
        zapAmountsFlashQueue.erase(zapAmountsFlashQueue.begin());
        xSemaphoreGive(zapMutex);

        // Click the buzzer zapAmount many times in a for loop with 100 ms delay
        for (int i = 0; i < zapAmount; i++) {
          click(225);
          delay(100);
        }
        doLightningFlash(zapAmount);

        // vTaskDelay(500 / portTICK_PERIOD_MS);
      }
      vTaskDelay(100 / portTICK_PERIOD_MS);
    }
  }
}

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
  Serial.println("npubHexString is |" + config_pubkey + "|");
  if(config_pubkey != "") {
    Serial.println("npub is specified");
    String* pubkeys = new String[1];  // Allocate memory dynamically
    pubkeys[0] = config_pubkey;
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
  // first disconnect from all relays
  nostrRelayManager.disconnect();
  Serial.println("Requesting Zap notifications");

  // split relays by comma into vector
  std::vector<String> relays;
  String relayStringCopy = config_relay;
  int commaIndex = relayStringCopy.indexOf(",");
  while (commaIndex != -1) {
    relays.push_back(relayStringCopy.substring(0, commaIndex));
    relayStringCopy = relayStringCopy.substring(commaIndex + 1);
    commaIndex = relayStringCopy.indexOf(",");
  }
  // add last item after last comma
  if (relayStringCopy.length() > 0) {
    relays.push_back(relayStringCopy);
  }

  // no need to convert to char* anymore
  nostr.setLogging(true);
  nostrRelayManager.setRelays(relays);
  nostrRelayManager.setMinRelaysAndTimeout(1,10000);

  // Set some event specific callbacks here
  Serial.println("Setting callbacks");
  nostrRelayManager.setEventCallback("ok", okEvent);
  nostrRelayManager.setEventCallback("connected", relayConnectedEvent);
  nostrRelayManager.setEventCallback("disconnected", relayDisonnectedEvent);
  nostrRelayManager.setEventCallback(9735, zapReceiptEvent);

  Serial.println("connecting");
  nostrRelayManager.connect();
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
      // quick double flash to show at max brughtness
      Serial.println("Max brightness");
      delay(500); // pause to let the user take their finger off the button
    }
  } else {
    lightBrightness = lightBrightness - 5;
    if (lightBrightness <= 0) {
      lightBrightness = 0;
      adjustLightingBrightnessUp = true;
      Serial.println("Min brightness");
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
}

/**
 * @brief Flash the LED
 * 
 * @param numberOfFlashes 
 */
void doLightningFlash(int numberOfFlashes) {

  Serial.println("Flashing " + String(numberOfFlashes) + " times");

  // turn lamp off
  analogWrite(ledPin, lightBrightness / 3);

  delay(100);

  for(int flash = 1; flash <= numberOfFlashes; flash++) {
    // turn the LED on
    analogWrite(ledPin, 255);

    // wait for the specified time, longer for the first flash and shorter for subsequent flashes
    // int flashDuration = 250 / flash * random(1,5);
    int flashDuration = 250;
    delay(250);

    // fast fade-out
    for (int i = 255; i >= lightBrightness / 3; i = i - 2) {
      analogWrite(ledPin, i);  // set the LED brightness
      delay(1);  // wait for a moment
    }
    // analogWrite(ledPin, lightBrightness / 3);
    delay(250);
  }

  // fade from lightBrightness / 3 to lightBrightness
  for (int i = lightBrightness / 3; i <= lightBrightness; i = i + 1) {
    analogWrite(ledPin, i); // set the LED brightness
    delay(1);  // wait for a moment
  }

  delay(250);

  // set led to brightness
  analogWrite(ledPin, lightBrightness);
}

/**
 * @brief Add a zap amount to the flash queue
 * 
 * @param zapAmountSats 
 */
void flashLightning(int zapAmountSats) {
  int flashCount = 1;
  // set flash count length of the number in the zap amount
  if (zapAmountSats > 0) {
    flashCount = floor(log10(zapAmountSats)) + 1;
  }

  // push to the flash queue
  xSemaphoreTake(zapMutex, portMAX_DELAY);
  zapAmountsFlashQueue.push_back(flashCount);
  xSemaphoreGive(zapMutex);
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

/**
 * @brief Get the Unix Timestamp 
 * 
 * @return unsigned long 
 */
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

/**
 * @brief Event callback for when a relay connects
 * 
 * @param key 
 * @param message 
 */
void relayConnectedEvent(const std::string& key, const std::string& message) {
  socketDisconnectedCount = 0;
  Serial.println("Relay connected: ");

  click(225);
  delay(100);
  click(225);
  delay(100);
  click(225);

  Serial.print(F("Requesting events:"));
  Serial.println(serialisedEventRequest);

  nostrRelayManager.broadcastEvent(serialisedEventRequest);
}

/**
 * @brief Event callback for when a relay disconnects
 * 
 * @param key 
 * @param message 
 */
void relayDisonnectedEvent(const std::string& key, const std::string& message) {
  Serial.println("Relay disconnected: ");
  socketDisconnectedCount++;
  // reboot after 3 socketDisconnectedCount subsequenet messages
  if(socketDisconnectedCount >= 3) {
    Serial.println("Too many socket disconnections. Restarting");
    // restart device
    ESP.restart();
  }
}

/**
 * @brief Event callback for when a relay sends an OK event
 * 
 * @param key 
 * @param payload 
 */
void okEvent(const std::string& key, const char* payload) {
    if(lastPayload != payload) { // Prevent duplicate events from multiple relays triggering the same logic
      lastPayload = payload;
      Serial.println("payload is: ");
      Serial.println(payload);
    }
}

/**
 * @brief Get the Bolt11 Invoice From Event object
 * 
 * @param jsonStr 
 * @return String 
 */
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

/**
 * @brief Get the Random Num object
 * 
 * @param min 
 * @param max 
 * @return uint16_t 
 */
uint16_t getRandomNum(uint16_t min, uint16_t max) {
  uint16_t rand  = (esp_random() % (max - min + 1)) + min;
  Serial.println("Random number: " + String(rand));
  return rand;
}

/**
 * @brief Event callback for when a relay sends a zap receipt event
 * 
 * @param key 
 * @param payload 
 */
void zapReceiptEvent(const std::string& key, const char* payload) {
    if(lastPayload != payload) { // Prevent duplicate events from multiple relays triggering the same logic, as we are using multiple relays, this is likely to happen
      lastPayload = payload;
      String bolt11 = getBolt11InvoiceFromEvent(payload);
      // Serial.println("BOLT11: " + bolt11);
      uint64_t amountInSatoshis = getAmountInSatoshis(bolt11);
      Serial.println("Zapped! " + String(amountInSatoshis));
      flashLightning(amountInSatoshis);
    }
}

/**
 * @brief Initialise the lamp
 * 
 */
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

  // start lamp control task
  xTaskCreatePinnedToCore(
    lampControlTask,   /* Task function. */
    "lampControlTask",     /* String with name of task. */
    5000,            /* Stack size in bytes. */
    NULL,             /* Parameter passed as input of the task */
    2,                /* Priority of the task. */
    NULL,             /* Task handle. */
    1);               /* Core where the task should run */
}

/**
 * @brief Click a piezo buzzer if used
 * 
 * @param period 
 */
void click(int period)
{
  if(!isBuzzerEnabled) {
    return;
  }
  for (int i = 0; i < CLICK_DURATION; i++)
  {
    digitalWrite(BUZZER_PIN, HIGH);
    delayMicroseconds(period); // Half period of 1000Hz tone.
    digitalWrite(BUZZER_PIN, LOW);
    delayMicroseconds(period); // Other half period of 1000Hz tone.
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("boot");

  pinMode(buttonPin, INPUT_PULLUP); // Set the button pin as INPUT
  if(isBuzzerEnabled) {
    pinMode(BUZZER_PIN, OUTPUT); // Set the buzzer pin as an output.
    click(225);
  }

  FlashFS.begin(FORMAT_ON_FAIL);
  // init spiffs
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  initLamp();

  bool triggerConfig = false;
  int timer = 0;
  while (timer < 2000)
  {
    analogWrite(ledPin, 255);
    
    Serial.println("Portal pin value is " + String(touchRead(portalPin)));
    Serial.println("Button pin is" + String(digitalRead(buttonPin)));
    if (
      touchRead(portalPin) < 60
      ||
      digitalRead(buttonPin) == LOW
      )
    {
        triggerConfig = true;
        timer = 5000;
    }

    timer = timer + 100;
    delay(150);
    analogWrite(ledPin, 0);
    delay(150);
  }

  readFiles(); // get the saved details and store in global variables

  if(triggerConfig == true || config_ssid == "" || config_ssid == "null") {
    Serial.println("Launch serial config");
    configOverSerialPort();
    hasInternetConnection = false;
  }
  else {
    WiFi.begin(config_ssid.c_str(), config_wifi_password.c_str());
    Serial.print("Connecting to WiFi");
    // connect for max of 3 seconds
    int wifiConnectTimer = 0;
    while (WiFi.status() != WL_CONNECTED && wifiConnectTimer < 3000) {
      delay(500);
      Serial.print(".");
      wifiConnectTimer = wifiConnectTimer + 500;
      hasInternetConnection = false;
    }
    if(WiFi.status() == WL_CONNECTED) {
      hasInternetConnection = true;
    }
    else {
      hasInternetConnection = false;
      configOverSerialPort();
    }

  }

  zapMutex = xSemaphoreCreateMutex();

  randomSeed(analogRead(0)); // Seed the random number generator

  createZapEventRequest();

  Serial.println("Connected to WiFi and got an IP");
  click(225);
  delay(100);
  click(225);
  connectToNostrRelays();

  // Set the LED to the desired intensity
  analogWrite(ledPin, lightBrightness);

}

String getJsonValue(JsonDocument &doc, const char* name)
{
    for (JsonObject elem : doc.as<JsonArray>()) {
        if (strcmp(elem["name"], name) == 0) {
            String value = elem["value"].as<String>();
            return value;
        }
    }
    return "";  // return empty string if not found
}
/**
 * @brief Read config from SPIFFS
 * 
 */
void readFiles()
{
    File paramFile = FlashFS.open(PARAM_FILE, "r");
    if (paramFile)
    {
        StaticJsonDocument<2500> doc;
        DeserializationError error = deserializeJson(doc, paramFile.readString());
        if(error){
            Serial.print("deserializeJson() failed: ");
            Serial.println(error.c_str());
            return;
        }
        if(config_ssid == "null"){ // check config_ssid is not set above
            config_ssid = getJsonValue(doc, "config_ssid");
            Serial.println("");
            Serial.println("config_ssid used from memory");
            Serial.println("config_ssid: " + config_ssid);
        }
        else{
            Serial.println("");
            Serial.println("config_ssid hardcoded");
            Serial.println("config_ssid: " + config_ssid);
        }
        if(config_wifi_password == "null"){ // check config_wifi_password is not set above
            config_wifi_password = getJsonValue(doc, "config_wifi_password");
            Serial.println("");
            Serial.println("config_wifi_password used from memory");
            Serial.println("config_wifi_password: " + config_wifi_password);
        }
        else{
            Serial.println("");
            Serial.println("config_wifi_password hardcoded");
            Serial.println("config_wifi_password: " + config_wifi_password);
        }
        if(config_pubkey == "null"){ // check nPubHex
            config_pubkey = getJsonValue(doc, "config_pubkey");
            Serial.println("");
            Serial.println("config_pubkey used from memory");
            Serial.println("config_pubkey: " + config_pubkey);
        }
        else{
            Serial.println("");
            Serial.println("config_pubkey hardcoded");
            Serial.println("config_pubkey: " + config_pubkey);
        }

        if(config_relay == "null"){ // check relays
            config_relay = getJsonValue(doc, "config_relay");
            Serial.println("");
            Serial.println("config_relays used from memory");
            Serial.println("config_relays: " + config_relay);
        }
        else{
            Serial.println("");
            Serial.println("config_relays hardcoded");
            Serial.println("config_relays: " + config_relay);
        }
    }
    paramFile.close();
}

bool lastInternetConnectionCheckTime = 0;

void loop() {
  // TESTING: fill the queue with some random zap amounts
  for (int i = 0; i < 3; i++) {
    zapAmountsFlashQueue.push_back(getRandomNum(1,3));
  }
  delay(30000);
  if (millis() - lastInternetConnectionCheckTime > 10000) {
    if(WiFi.status() == WL_CONNECTED) {
        lastInternetConnectionState = true;
      } else {
        lastInternetConnectionState = false;
      }
    }

  nostrRelayManager.loop();
  nostrRelayManager.broadcastEvents();

  // reboot every hour
  if (millis() > 3600000) {
    Serial.println("Rebooting");
    ESP.restart();
  }
}
