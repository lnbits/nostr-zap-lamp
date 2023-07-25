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


#include "NotoSansBold36.h"
#define AA_FONT NotoSansBold36

int ledPin = 13; // Pin number where the LED is connected
int minFlashDelay = 100; // Minimum delay between flashes (in milliseconds)
int maxFlashDelay = 5000; // Maximum delay between flashes (in milliseconds)

const char* ssid     = "wubwub"; // wifi SSID here
const char* password = "blob19750405blob"; // wifi password here

NostrEvent nostr;
NostrRelayManager nostrRelayManager;
NostrQueueProcessor nostrQueue;

String serialisedEventRequestOptions;

NostrRequestOptions* eventRequestOptions;

bool hasSentEvent = false;

String preimageHex = "";
String preimageHashHex = "";

void generatePreimageAndPreimageHash() {
  uint8_t preimage[32];
  esp_random() % 256;
  for (int i = 0; i < sizeof(preimage); i++) {
      preimage[i] = esp_random() % 256;
  }

  preimageHex = toHex(preimage, 32);
  Serial.println("Preimage hex: " + preimageHex);

  byte preimageHash[32] = { 0 }; // hash
  int preimageHashLen = 0;
  preimageHashLen = sha256(preimage, sizeof(preimage), preimageHash);
  preimageHashHex = toHex(preimageHash, preimageHashLen);
  Serial.println("Preimage hash hex: " + String(preimageHashHex));
}

void flashLightning(int zapAmountSats) {
  int flashCount = 1;
  // set flash count length of the number in the zap amount
  if (zapAmountSats > 0) {
    flashCount = floor(log10(zapAmountSats)) + 1;
  }
  Serial.println("Zap amount: " + String(zapAmountSats) + " sats");
  Serial.println("Flashing " + String(flashCount) + " times");

  int flashDuration = random(10, 150); // Generate random duration for each flash (20 to 100 milliseconds)

  for (int i = 0; i < flashCount; i++) {
    digitalWrite(ledPin, HIGH);
    delay(flashDuration);

    digitalWrite(ledPin, LOW);
    delay(random(50, 200));
  }

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
  Serial.println(serialisedEventRequestOptions);

  nostrRelayManager.broadcastEvent(serialisedEventRequestOptions);
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

void setup() {
  Serial.begin(115200);

  pinMode(ledPin, OUTPUT); // Set the LED pin as OUTPUT
  randomSeed(analogRead(0)); // Seed the random number generator
  
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
  
  // nostrRelayManager.requestEvents(eventRequestOptions);

  // We store this here for sending this request again if a socket reconnects
  serialisedEventRequestOptions = "[\"REQ\", \"" + nostrRelayManager.getNewSubscriptionId() + "\"," + eventRequestOptions->toJson() + "]";

  delete eventRequestOptions;
}

void loop() {
  nostrRelayManager.loop();
  nostrRelayManager.broadcastEvents();
}