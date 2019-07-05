#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRutils.h>
#include <ArduinoJson.h>

#ifndef STASSID
#define STASSID "your-wifi-ssid"
#define STAPSK  "your-wifi-password"
#endif

/* 
 * The pin on which the IR receiver is connected. 5 corresponds to GPIO5 and on some boards it is labeled D1.
 */
const uint16_t ir_receiverPin = 5;

/* 
 * The IR codes that is for volume up and down. The value can be read via the serial monitor.
 */
const char* ircode_volumeUp = "E0E0E01F";
const char* ircode_volumeDown = "E0E0D02F";

const char* ssid     = STASSID;
const char* password = STAPSK;

/* 
 * Timeout for http requests to the device.
 */
const uint16_t response_timeout = 500;

/* 
 * If the volume up/down button is continously pressed within 3 seconds, 
 * we do not fetch the current volume from the device.
 */
const uint16_t dontFetchLevelsWithinMillis = 3000;

/*
 * Increases volume 2 times for each button press
 */
const uint16_t levelStep = 2;

/* 
 * The baud rate which serial data are sent
 */
const uint32_t kBaudRate = 115200;


/////////////////////////////////

const uint16_t ir_captureBufferSize = 1024;
const uint16_t ir_minUnknownSize = 12;
const uint8_t ir_timeout = 15;

const uint16_t beoPort = 8080;

String beoHost = "";

int16_t currentLevel = 40;
int16_t maxLevel = 60;
int16_t minLevel = 0;

unsigned long lastLevelChange = 0;

char hostName[16] = {0};

IRrecv irrecv(ir_receiverPin, ir_captureBufferSize, ir_timeout, true);
decode_results results;
String ir_receivedCode;

WiFiClient client;

void setup() {
  Serial.begin(kBaudRate, SERIAL_8N1, SERIAL_TX_ONLY);

  sprintf(hostName, "ESP_%06X", ESP.getChipId());
  Serial.print("Hostname: ");
  Serial.println(hostName);
  WiFi.hostname(hostName);
  
  Serial.print("Connecting to ");
  Serial.println(ssid);

  /* Explicitly set the ESP8266 to be a WiFi-client, otherwise, it by default,
     would try to act as both a client and an access-point and could cause
     network-issues with your other WiFi-devices on your WiFi-network. */
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  if (!MDNS.begin(hostName)) {
    Serial.println("Error setting up MDNS responder!");
  }

  Serial.println("Finding BeoSound Core");
  while(beoHost == "") {
    delay(500);
    Serial.print(".");

    if (MDNS.queryService("beoremote", "tcp") > 0) {
      Serial.println();
      Serial.print("Found ");
      Serial.print(MDNS.hostname(0));
      Serial.println("!");
      beoHost = MDNS.IP(0).toString();
    }
  }
  
  #if DECODE_HASH
    // Ignore messages with less than minimum on or off pulses.
    irrecv.setUnknownThreshold(ir_minUnknownSize);
  #endif                  // DECODE_HASH
  
  irrecv.enableIRIn();  // Start the IR receiver
  
  Serial.println("Ready for IR commands");
  Serial.println();
  Serial.println();
}

void loop() {
  if (irrecv.decode(&results)) {
    ir_receivedCode = resultToHexidecimal(&results);
    Serial.println("Received IR command: ");
    Serial.println(ir_receivedCode);
    if(ir_receivedCode == ircode_volumeUp || ir_receivedCode == ircode_volumeDown) {
      if(!client.connected()) {
        Serial.println("Connecting to BeoSound Core");
        if (!client.connect(beoHost, beoPort)) {
          Serial.println("Connection failed");
          return;
        }
      }

      // If the last request was very recent we hope that the settings are the same
      if(millis() - lastLevelChange > dontFetchLevelsWithinMillis) {
        //get current volume and settings
        if (client.connected()) {
          client.print(String("GET ") + "/BeoZone/Zone/Sound/Volume/Speaker HTTP/1.1\r\n" +
                 "Connection: keep-alive\r\n" +
                 "Accept: */*\r\n\r\n");
          client.keepAlive();
        }
        
        // wait for data to be available
        unsigned long timeout = millis();
        while (client.available() == 0) {
          if (millis() - timeout > response_timeout) {
            Serial.println(">>> Client Timeout !");
            client.stop();
            return;
          }
        }
        // Check HTTP status
        if(client.available()) {
          String status = client.readStringUntil('\r');
          if (!status.equals("HTTP/1.1 200 OK")) {
            Serial.print(F("Unexpected response: "));
            Serial.println(status);
            client.stop();
            return;
          }
          
          // Skip HTTP headers
          char endOfHeaders[] = "\r\n\r\n";
          if (!client.find(endOfHeaders)) {
            Serial.println(F("Invalid response"));
            client.stop();
            return;
          }
          
          // Allocate the JSON document
          // Use arduinojson.org/v6/assistant to compute the capacity.
          const size_t capacity = 2*JSON_ARRAY_SIZE(1) + 4*JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5) + 200;
          DynamicJsonDocument doc(capacity);
          
          // Parse JSON object
          DeserializationError error = deserializeJson(doc, client);
          if (error) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.c_str());
            return;
          }
  
          maxLevel = doc["speaker"]["range"]["maximum"].as<int16_t>();
          minLevel = doc["speaker"]["range"]["minimum"].as<int16_t>();
          currentLevel = doc["speaker"]["level"].as<int16_t>();
          Serial.println(String("Max level: ") + maxLevel);
          Serial.println(String("Min level: ") + minLevel);
          Serial.println(String("Current level: ") + currentLevel);
        }
      }
      
      if(ir_receivedCode == ircode_volumeUp) {
        Serial.println("Step Volume Up");
        currentLevel += levelStep;
      }
      if(ir_receivedCode == ircode_volumeDown) {
        Serial.println("Step Volume Down");
        currentLevel -= levelStep;
      }
      if(currentLevel > maxLevel) {
        currentLevel = maxLevel;
      }
      if(currentLevel < minLevel) {
        currentLevel = minLevel;
      }
      
      Serial.println("Send command to set level: ");
      Serial.println(currentLevel);
      if (client.connected()) {
        client.print(String("PUT ") + "/BeoZone/Zone/Sound/Volume/Speaker/Level HTTP/1.1\r\n" +
               "Content-Type: application/json\r\n" +
               "Accept: */*\r\n" +
               "Connection: keep-alive\r\n" +
               "Content-Length: 16\r\n\r\n" +
               "{\"level\":" + currentLevel + "}\r\n\r\n");
      }
      client.keepAlive();
      
      // wait for data to be available
      unsigned long timeout = millis();
      while (client.available() == 0) {
        if (millis() - timeout > response_timeout) {
          Serial.println(">>> Client Timeout !");
          lastLevelChange = 0; //Fetch settings again, maybe they are faulty?
          client.stop();
          return;
        }
      }
      
      if(client.available()) {
        String status = client.readStringUntil('\r');
        if (!status.equals("HTTP/1.1 200 OK")) {
          Serial.print(F("Unexpected response: "));
          Serial.println(status);
          lastLevelChange = 0; //Fetch settings again, maybe they are faulty?
          client.stop();
          return;
        }
      }
      //Throw away rest of the response
      while (client.available()) {client.read();}
      
      client.keepAlive();
      lastLevelChange = millis();
    }
    
  }
  yield();  // prevent watchdog (WDT) reset
}
