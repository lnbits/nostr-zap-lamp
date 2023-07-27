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

#include "NotoSansBold36.h"
#define AA_FONT NotoSansBold36

int ledPin = 13; // Pin number where the LED is connected
int buttonPin = 4; // Pin number where the button is connected
int minFlashDelay = 100; // Minimum delay between flashes (in milliseconds)
int maxFlashDelay = 5000; // Maximum delay between flashes (in milliseconds)
int lightBrightness = 50; // The brightness of the LED (0-255)

const char* ssid     = "Maddox Guest"; // wifi SSID here
const char* password = "MadGuest1"; // wifi password here

NostrEvent nostr;
NostrRelayManager nostrRelayManager;
NostrQueueProcessor nostrQueue;

String serialisedEventRequest;

NostrRequestOptions* eventRequestOptions;

bool hasSentEvent = false;

String preimageHex = "";
String preimageHashHex = "";

/**
 * @brief change lamp brightness
 * 
 */
void changeBrightness() {
  lightBrightness = lightBrightness + 20;
  if (lightBrightness > 255) {
    lightBrightness = 0;
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
  delay(250);
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

  fadeOutFlash(2);
  fadeOutFlash(5);
  fadeOutFlash(10);

  delay(50);

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

  fadeOutFlash(15);
  fadeOutFlash(5);

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

  // get brightness value from spiffs brightness.txt
  File file = SPIFFS.open("/brightness.txt");
  if(!file){
    Serial.println("Failed to open file for reading");
    return;
  }
  String brightnessStr = file.readStringUntil('\n');
  file.close();
  lightBrightness = brightnessStr.toInt();

  // Set the LED pin as OUTPUT
  pinMode(ledPin, OUTPUT);

  // Set the LED to the desired intensity
  analogWrite(ledPin, lightBrightness);
}

void setup() {
  Serial.begin(115200);

  randomSeed(analogRead(0)); // Seed the random number generator

  // init spiffs
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  initLamp();

  // Set the button pin as INPUT
  pinMode(buttonPin, INPUT_PULLUP);
  
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  signalWithLightning(3,100);

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Create the REQ
  eventRequestOptions = new NostrRequestOptions();
  // Populate kinds
  int kinds[] = {9735};
  eventRequestOptions->kinds = kinds;
  eventRequestOptions->kinds_count = sizeof(kinds) / sizeof(kinds[0]);

  // // Populate #p
  // String p[] = {npubHex};
  // eventRequestOptions->p = p;
  // eventRequestOptions->p_count = sizeof(p) / sizeof(p[0]);;

  eventRequestOptions->limit = 0;

  Serial.println("Requesting Zap notifications");

  const char *const relays[] = {
      "relay.damus.io",
      "nostr.mom",
      "relay.nostr.bg"
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
  
  // We store this here for sending this request again if a socket reconnects
  serialisedEventRequest = "[\"REQ\", \"" + nostrRelayManager.getNewSubscriptionId() + "\"," + eventRequestOptions->toJson() + "]";

  delete eventRequestOptions;
}

void loop() {
  nostrRelayManager.loop();

  // watch for button press and call changeBrightness
  if (digitalRead(buttonPin) == LOW) {
    changeBrightness();
  }
}