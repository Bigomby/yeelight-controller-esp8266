#include "messages.hpp"

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <FS.h>
#include <WiFiManager.h>
#include <kinton.hpp>

// Configuration
const char *CONFIG_FILE = "/kinton.config";
const char *FLEET_KEY = "499d3780-855d-4702-b86e-016c9eabcc93";
const char *YEELIGHT_IP = "192.168.1.169";
const uint16_t YEELIGHT_PORT = 55443;

// Globals
WiFiClient kinton_client;
WiFiClient yeelight_client;
KintonMQTT kinton(kinton_client, "yeelight-controller");

/**
 * Initializes the kinton module by loading the credentials from some storage
 * provider, it could be an EEPROM, or an SD card. In this case is the ESP8266
 * file system. If no crentials are found on the filesystem, then perform
 * a registration to obtain new credentials from the API.
 */
bool init_kinton_config() {
  char device_uuid[37];
  char device_secret[65];

  // Try to load the credentials from the file system
  File kinton_config = SPIFFS.open(CONFIG_FILE, "r");
  if (kinton_config) {
    kinton_config.readBytes(device_uuid, 36);
    kinton_config.readStringUntil('\n');
    kinton_config.readBytes(device_secret, 64);

    device_uuid[36] = '\0';
    device_secret[64] = '\0';
    kinton_config.close();

    kinton.setCredentials(device_uuid, device_secret);
  } else {

    // If credentials not found, then perform the registration
    if (kinton.registerDevice(FLEET_KEY)) {

      // Save the credentials for later use
      File kinton_config = SPIFFS.open(CONFIG_FILE, "w");
      if (kinton_config) {
        kinton_config.println(kinton.getDeviceUUID());
        kinton_config.println(kinton.getDeviceSecret());
        kinton_config.close();
      } else {
        Serial.println("Error storing credentials");
      }
    } else {
      return false;
    }
  }

  return true;
}

void toggle_lights(byte *payload, unsigned int length) {
  if (length == 1 && payload[0] == 'T') {
    Serial.println("Toggle lights");

    if (!yeelight_client.connect(YEELIGHT_IP, YEELIGHT_PORT)) {
      Serial.println("connection failed");
      return;
    }

    yeelight_client.print(YEELIGHT_TOGGLE);
  }
}

void setup() {
  String lights_topic;

  Serial.begin(115200);
  SPIFFS.begin();

  // This is only necessary the first time to format the file system, then
  // comment these lines

  // Serial.println("Formatting FS...");
  // SPIFFS.format();
  // Serial.println("Done");

  String device_topic;
  WiFiManager wifiManager;
  wifiManager.autoConnect("Yeelight Controller");

  // If it's not possible to initiate the kinton connection, then halt the
  // device
  if (!init_kinton_config()) {
    goto halt;
  }

  lights_topic += FLEET_KEY;
  lights_topic += "/lights";

  kinton.on(lights_topic.c_str(), toggle_lights);

  return;

halt:
  Serial.println("Halted");
  while (true) {
    delay(1);
  }
}

void loop() {
  // Call this function on every loop iteration
  if (!kinton.loop()) {
    delay(5000);
  }
}
