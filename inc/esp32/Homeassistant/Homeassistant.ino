#include <WiFi.h>
#include "BLEDevice.h"

// WiFi credentials
const char* ssid = "Stan_moto";
const char* password = "Stan1203";

// Server host/path
const char* serverHost = "stan.1pc.nl";
const char* serverPath = "/Train/inc/php/data.php";

char deviceId[20];  // MAC address string

// BLE setup
// MAC address of the M5 Stack (update this to match your M5's MAC)
BLEAddress m5Address("");

bool findM5() {
  Serial.println("Scanning for M5 LEGO Bridge...");

  BLEScan* scan = BLEDevice::getScan();
  scan->setActiveScan(true);
  BLEScanResults* rs = scan->start(5);

  for (int i = 0; i < rs.getCount(); i++) {
    BLEAdvertisedDevice dev = rs.getDevice(i);
    if (dev.getName() == "M5 LEGO Bridge") {
      Serial.print("FOUND M5 at: ");
      Serial.println(dev.getAddress().toString().c_str());
      m5Address = dev.getAddress();
      return true;
    }
  }

  Serial.println("M5 LEGO Bridge not found!");
  return false;
}

// Nordic UART Service (NUS) UUIDs used by M5 BLE Bridge
static BLEUUID uartServiceUUID("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
// RX characteristic (PC→M5): write commands here
static BLEUUID uartRxCharUUID("6E400002-B5A3-F393-E0A9-E50E24DCCA9E");
// TX characteristic (M5→PC): receive responses here
static BLEUUID uartTxCharUUID("6E400003-B5A3-F393-E0A9-E50E24DCCA9E");

BLERemoteCharacteristic* trainRxChar = nullptr;  // For writing TO M5
BLERemoteCharacteristic* trainTxChar = nullptr;  // For reading FROM M5
BLEClient* bleClient = nullptr;

char lastBleResponse[128];
bool bleResponseReady = false;

// ---------- BLE ----------

void notifyCallback(BLERemoteCharacteristic* c, uint8_t* data, size_t length, bool isNotify) {
  Serial.print("BLE Notify received (");
  Serial.print(length);
  Serial.print(" bytes): ");

  if (length >= sizeof(lastBleResponse)) length = sizeof(lastBleResponse) - 1;
  memcpy(lastBleResponse, data, length);
  lastBleResponse[length] = '\0';
  bleResponseReady = true;

  // Print as hex for debugging
  for (size_t i = 0; i < length; i++) {
    Serial.printf("%02X ", data[i]);
  }
  Serial.println();

  Serial.print("BLE Notify string: ");
  Serial.println(lastBleResponse);
}

bool connectToTrain() {
  if (!bleClient) bleClient = BLEDevice::createClient();
  if (bleClient->isConnected()) return true;

  // If MAC unknown → scan once
  if (m5Address.toString() == "") {
    if (!findM5()) return false;
  }

  Serial.print("Connecting to M5: ");
  Serial.println(m5Address.toString().c_str());

  if (!bleClient->connect(m5Address)) {
    Serial.println("Connect failed");
    m5Address = BLEAddress("");  // force rescan next time
    return false;
  }

  BLERemoteService* svc = bleClient->getService(uartServiceUUID);
  if (!svc) {
    Serial.println("BLE service not found");
    bleClient->disconnect();
    return false;
  }

  // RX = ESP32 writes TO M5 (M5 receives)
  trainRxChar = svc->getCharacteristic(uartRxCharUUID);
  // TX = ESP32 reads FROM M5 (M5 transmits)
  trainTxChar = svc->getCharacteristic(uartTxCharUUID);

  if (!trainRxChar || !trainTxChar) {
    Serial.println("BLE characteristics not found");
    bleClient->disconnect();
    return false;
  }

  // Register for notifications on the TX characteristic (M5→ESP32)
  trainTxChar->registerForNotify(notifyCallback);

  Serial.println("BLE connected to M5 Stack!");
  return true;
}

bool sendBLE(const char* msg) {
  if (!connectToTrain()) return false;

  // Write to RX characteristic (ESP32→M5)
  trainRxChar->writeValue((uint8_t*)msg, strlen(msg));
  Serial.print("BLE sent: ");
  Serial.println(msg);
  return true;
}

bool getLastBLEResponse(char* buf, size_t bufsize) {
  if (!bleResponseReady) return false;
  strncpy(buf, lastBleResponse, bufsize);
  buf[bufsize - 1] = '\0';
  bleResponseReady = false;
  return true;
}

// ---------- URL Encoding ----------

void urlEncode(const char* str, char* out, size_t outSize) {
  size_t j = 0;
  for (size_t i = 0; str[i] && j + 4 < outSize; i++) {
    char c = str[i];
    if (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('0' <= c && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
      out[j++] = c;
    } else if (c == ' ') {
      out[j++] = '+';
    } else {
      snprintf(out + j, 4, "%%%02X", (unsigned char)c);
      j += 3;
    }
  }
  out[j] = '\0';
}

// ---------- HTTP Request ----------

bool httpGet(const char* query, char* response, size_t respSize) {
  WiFiClient client;
  if (!client.connect(serverHost, 80)) return false;

  client.printf("GET %s?%s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
                serverPath, query, serverHost);

  unsigned long timeout = millis() + 5000;
  while (!client.available() && millis() < timeout) delay(10);
  if (!client.available()) return false;

  // skip headers
  while (client.available()) {
    String line = client.readStringUntil('\n');
    if (line == "\r" || line == "\n") break;
  }

  // read body
  size_t idx = 0;
  while (client.available() && idx < respSize - 1) {
    response[idx++] = client.read();
  }
  response[idx] = '\0';
  return true;
}

// ---------- Server communication ----------

void postResponse(const char* msg) {
  char encMsg[256], encDev[32];
  urlEncode(msg, encMsg, sizeof(encMsg));
  urlEncode(deviceId, encDev, sizeof(encDev));

  char query[512];
  snprintf(query, sizeof(query), "action=post_response&device=%s&msg=%s", encDev, encMsg);

  char resp[128];
  if (httpGet(query, resp, sizeof(resp))) {
    Serial.print("Posted response: ");
    Serial.println(resp);
  } else {
    Serial.println("Post response failed");
  }
}

void sendStatus(const char* type, const char* msg) {
  char payload[256];
  snprintf(payload, sizeof(payload), "STATUS|type=%s;msg=%s", type, msg);
  postResponse(payload);
}

void registerDevice() {
  char encDev[32];
  urlEncode(deviceId, encDev, sizeof(encDev));
  char query[128];
  snprintf(query, sizeof(query), "action=connect&device=%s", encDev);

  char resp[128];
  if (httpGet(query, resp, sizeof(resp))) {
    Serial.print("Register response: ");
    Serial.println(resp);
  } else {
    Serial.println("Register failed");
  }
}

// ---------- Server polling ----------

void checkMessages() {
  char encDev[32];
  urlEncode(deviceId, encDev, sizeof(encDev));
  char query[128];
  snprintf(query, sizeof(query), "action=get_messages&device=%s", encDev);

  char payload[512];
  if (!httpGet(query, payload, sizeof(payload))) {
    Serial.println("get_messages failed");
    return;
  }

  char* line = strtok(payload, "\n");
  while (line) {
    if (strlen(line) == 0) {
      line = strtok(nullptr, "\n");
      continue;
    }
    Serial.print("CMD: ");
    Serial.println(line);

    if (strncmp(line, "ORDER|", 6) == 0) {
      sendStatus("received", "order received");

      if (!sendBLE(line)) {
        sendStatus("error", "ble_send_failed");
        postResponse("ExecutedOrder|error=ble_send_failed");
        line = strtok(nullptr, "\n");
        continue;
      }

      Serial.println("Order forwarded to train via BLE");

      char bleResp[128];
      bleResp[0] = '\0';
      unsigned long timeout = millis() + 5000;
      while (millis() < timeout) {
        if (getLastBLEResponse(bleResp, sizeof(bleResp))) break;
        delay(50);
      }

      if (strlen(bleResp) == 0) {
        sendStatus("error", "train_no_response");
        postResponse("ExecutedOrder|error=train_no_response");
      } else {
        Serial.print("Train response: ");
        Serial.println(bleResp);
        postResponse(bleResp);
        sendStatus("done", "order_complete");
      }
    } else {
      char resp[256];
      snprintf(resp, sizeof(resp), "Executed: %s", line);
      postResponse(resp);
    }

    line = strtok(nullptr, "\n");
  }
}

unsigned long lastPoll = 0;
unsigned long lastHeartbeat = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 60) {
    delay(500);
    Serial.print('.');
    retries++;
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi failed");
    return;
  }

  Serial.print("WiFi connected: ");
  Serial.println(WiFi.localIP());

  snprintf(deviceId, sizeof(deviceId), "%s", WiFi.macAddress().c_str());
  Serial.print("Device ID: ");
  Serial.println(deviceId);

  registerDevice();

  BLEDevice::init("");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    delay(2000);
    return;
  }

  unsigned long now = millis();

  if (now - lastPoll > 5000) {
    checkMessages();
    lastPoll = now;
  }

  if (now - lastHeartbeat > 15000) {
    char encDev[32];
    urlEncode(deviceId, encDev, sizeof(encDev));
    char query[128];
    snprintf(query, sizeof(query), "action=heartbeat&device=%s", encDev);
    char resp[128];
    httpGet(query, resp, sizeof(resp));
    lastHeartbeat = now;
  }
}