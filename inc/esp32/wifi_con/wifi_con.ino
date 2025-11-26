#include <WiFi.h>
#include <M5Unified.h>
#include "BLEDevice.h"
#include <HTTPClient.h>

const char* B_BLE_NAME = "M5 LEGO Bridge";
const char* serverUrl = "http://stan.1pc.nl/Train/inc/php/data.php";
const char* wifi_ssid = "Stan_moto";
const char* wifi_pass = "Stan1203";

BLEClient* pClient;
BLERemoteCharacteristic* pRemoteTx;  // TX from A → B
BLERemoteCharacteristic* pRemoteRx;  // RX from B → A

void setup() {
  M5.begin();
  Serial.begin(115200);

  // 1. Wi-Fi connection
  WiFi.begin(wifi_ssid, wifi_pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to Wi-Fi");

  // 2. BLE init
  BLEDevice::init("ESP32_A_Client");
  connectToB();
}

void loop() {
  // Poll PHP server for messages for B
  String messages = getMessagesFromServer("ESP32_B");
  if (messages.length() > 0) {
    Serial.println("Messages for B: " + messages);
    sendToBLE(messages);
  }

  // Check for BLE responses from B
  if (pRemoteRx) {
    std::string valStd = pRemoteRx->readValue();
    if (valStd.length() > 0) {
      String val = String(valStd.c_str());
      Serial.println("Response from B: " + val);
      sendResponseToServer("ESP32_B", val);
    }
  }

  delay(1000);
}

void connectToB() {
  // Scan and connect to BLE device
  BLEScan* pScan = BLEDevice::getScan();
  pScan->setActiveScan(true);
  BLEScanResults* results = pScan->start(5);
  for (int i = 0; i < results.getCount(); i++) {
    BLEAdvertisedDevice dev = results.getDevice(i);
    if (dev.getName() == B_BLE_NAME) {
      pClient = BLEDevice::createClient();
      pClient->connect(&dev);
      BLERemoteService* pService = pClient->getService(dev.getServiceUUID());
      if (pService) {
        pRemoteTx = pService->getCharacteristic(dev.getCharacteristicUUID());  // TX from A → B
        pRemoteRx = pService->getCharacteristic(dev.getCharacteristicUUID());  // RX from B → A
      }
      break;
    }
  }
}

void sendToBLE(String msg) {
  if (pRemoteTx) {
    pRemoteTx->writeValue(msg.c_str(), msg.length());
  }
}

String getMessagesFromServer(String device) {
  if (WiFi.status() != WL_CONNECTED) return "";
  HTTPClient http;
  String url = String(serverUrl) + "?action=get_messages&device=" + device + "&format=json";
  http.begin(url);
  int code = http.GET();
  String payload = "";
  if (code == 200) payload = http.getString();
  http.end();
  return payload;
}

void sendResponseToServer(String device, String msg) {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  String url = String(serverUrl) + "?action=post_response&device=" + device + "&msg=" + msg;
  http.begin(url);
  http.GET();
  http.end();
}
