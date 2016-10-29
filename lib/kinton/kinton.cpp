// Copyright 2016 Diego Fernández Barrera
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "kinton.hpp"

struct Handler {
  KintonMQTT *kinton = NULL;

  void operator()(char *topic, byte *payload, unsigned int length) {
    for (int i = 0; i < MAX_TOPICS; i++) {
      if (NULL != kinton->topics[i] && 0 == strcmp(topic, kinton->topics[i])) {
        kinton->callbacks[i](payload, length);
        break;
      }
    }
  }
};

KintonMQTT::KintonMQTT(WiFiClient client, const char *mqtt_id) {
  this->mqtt_id = strdup(mqtt_id);
  this->device_uuid = NULL;
  this->device_secret = NULL;
  for (int i = 0; i < MAX_TOPICS; i++) {
    this->topics[i] = NULL;
  }

  Handler handler;
  handler.kinton = this;
  this->client = new PubSubClient(this->kinton_ip, this->kinton_port, client);
  this->client->setCallback(handler);
}

bool KintonMQTT::registerDevice(const char *fleet_key) {
  HTTPClient http;
  String register_url;
  register_url += KINTON_API;
  register_url += fleet_key;
  register_url += KINTON_API_REGISTER_METHOD;

  Serial.printf("Registering against: %s\n", register_url.c_str());

  http.begin(register_url.c_str());
  int httpCode = http.POST("");
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("Got response code: %d\n", httpCode);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  StaticJsonBuffer<256> jsonBuffer;
  JsonObject &root = jsonBuffer.parseObject(payload);
  if (!root.success()) {
    return false;
  }

  this->device_uuid = strdup(root["uuid"]);
  this->device_secret = strdup(root["secret"]);

  return true;
}

const char *KintonMQTT::getDeviceUUID() { return this->device_uuid; }

const char *KintonMQTT::getDeviceSecret() { return this->device_secret; }

void KintonMQTT::setCredentials(const char *device_uuid,
                                const char *device_secret) {
  this->device_uuid = strdup(device_uuid);
  this->device_secret = strdup(device_secret);
}

bool KintonMQTT::connect() {
  if (!this->client->connect(this->mqtt_id, this->device_uuid,
                             this->device_secret)) {
    Serial.println("Error connecting to Kinton");
    Serial.println(this->device_uuid);
    Serial.println(this->device_secret);
    return false;
  }

  Serial.println("Connected to Kinton");

  for (int i = 0; i < MAX_TOPICS; i++) {
    if (this->topics[i] != NULL) {
      if (!this->client->subscribe(this->topics[i])) {
        return false;
      }
      Serial.print("Subscribed to: ");
      Serial.println(this->topics[i]);
      break;
    }
  }

  return true;
}

bool KintonMQTT::loop() {
  if (!this->client->connected()) {
    if (!connect()) {
      return false;
    }
  }

  return this->client->loop();
}

void KintonMQTT::on(const char *topic,
                    void (*cb)(byte *payload, unsigned int length)) {
  for (int i = 0; i < MAX_TOPICS; i++) {
    if (this->topics[i] == NULL) {
      this->topics[i] = strdup(topic);
      this->callbacks[i] = cb;
      break;
    }
  }
}
