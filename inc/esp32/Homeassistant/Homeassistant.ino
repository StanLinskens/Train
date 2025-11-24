#include <WiFi.h>
#include "BLEDevice.h"

// WiFi credentials
const char* ssid = "Stan_moto";
const char* password = "Stan1203";

// Server host/path
const char* serverHost = "stan.1pc.nl";
const char* serverPath = "/Train/inc/php/data.php";

char deviceId[20]; // MAC address string

// BLE setup
static BLEAddress trainAddress("f4:65:0B:33:79:F2");
static BLEUUID serviceUUID("1234");
static BLEUUID charUUID("5678");

BLERemoteCharacteristic* trainChar = nullptr;
BLEClient* bleClient = nullptr;

char lastBleResponse[128];
bool bleResponseReady = false;

// ---------- BLE ----------

void notifyCallback(BLERemoteCharacteristic* c, uint8_t* data, size_t length, bool isNotify) {
  if (length >= sizeof(lastBleResponse)) length = sizeof(lastBleResponse) - 1;
  memcpy(lastBleResponse, data, length);
  lastBleResponse[length] = '\0';
  bleResponseReady = true;
  Serial.print("Received BLE notify: "); Serial.println(lastBleResponse);
}

bool connectToTrain() {
  if (!bleClient) bleClient = BLEDevice::createClient();
  if (bleClient->isConnected()) return true;

  Serial.println("Connecting to train...");
  if (!bleClient->connect(trainAddress)) {
    Serial.println("BLE connect failed");
    return false;
  }

  BLERemoteService* svc = bleClient->getService(serviceUUID);
  if (!svc) { Serial.println("BLE service not found"); return false; }

  trainChar = svc->getCharacteristic(charUUID);
  if (!trainChar) { Serial.println("BLE characteristic not found"); return false; }

  trainChar->registerForNotify(notifyCallback);
  Serial.println("BLE connected to Train Controller!");
  return true;
}

bool sendBLE(const char* msg) {
  if (!connectToTrain()) return false;
  trainChar->writeValue((uint8_t*)msg, strlen(msg));
  Serial.print("BLE sent: "); Serial.println(msg);
  return true;
}

bool getLastBLEResponse(char* buf, size_t bufsize) {
  if (!bleResponseReady) return false;
  strncpy(buf, lastBleResponse, bufsize);
  buf[bufsize-1] = '\0';
  bleResponseReady = false;
  return true;
}

// ---------- URL Encoding ----------

void urlEncode(const char* str, char* out, size_t outSize) {
  size_t j = 0;
  for (size_t i = 0; str[i] && j + 4 < outSize; i++) {
    char c = str[i];
    if (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('0' <= c && c <= '9') || c=='-'||c=='_'||c=='.'||c=='~') {
      out[j++] = c;
    } else if (c==' ') {
      out[j++] = '+';
    } else {
      snprintf(out+j, 4, "%%%02X", (unsigned char)c);
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
  if (httpGet(query, resp, sizeof(resp))) Serial.print("Posted response: "), Serial.println(resp);
  else Serial.println("Post response failed");
}

void sendStatus(const char* type, const char* msg) {
  char payload[256];
  snprintf(payload, sizeof(payload), "STATUS|type=%s;msg=%s", type, msg);
  postResponse(payload);
}

void registerDevice() {
  char encDev[32]; urlEncode(deviceId, encDev, sizeof(encDev));
  char query[128];
  snprintf(query, sizeof(query), "action=connect&device=%s", encDev);

  char resp[128];
  if (httpGet(query, resp, sizeof(resp))) Serial.print("Register response: "), Serial.println(resp);
  else Serial.println("Register failed");
}

// ---------- Server polling ----------

void checkMessages() {
  char encDev[32]; urlEncode(deviceId, encDev, sizeof(encDev));
  char query[128];
  snprintf(query, sizeof(query), "action=get_messages&device=%s", encDev);

  char payload[512];
  if (!httpGet(query, payload, sizeof(payload))) { Serial.println("get_messages failed"); return; }

  char* line = strtok(payload, "\n");
  while (line) {
    if (strlen(line) == 0) { line = strtok(nullptr, "\n"); continue; }
    Serial.print("CMD: "); Serial.println(line);

    if (strncmp(line, "ORDER|", 6) == 0) {
      sendStatus("received", "order received");

      if (!sendBLE(line)) {
        sendStatus("error", "ble_send_failed");
        postResponse("ExecutedOrder|error=ble_send_failed");
        return;
      }

      Serial.println("Order forwarded to train via BLE");

      char bleResp[128];
      unsigned long timeout = millis() + 5000;
      while (millis() < timeout) {
        if (getLastBLEResponse(bleResp, sizeof(bleResp))) break;
        delay(50);
      }

      if (strlen(bleResp) == 0) {
        sendStatus("error", "train_no_response");
        postResponse("ExecutedOrder|error=train_no_response");
      } else {
        Serial.print("Train response: "); Serial.println(bleResp);
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
    delay(500); Serial.print('.');
    retries++;
  }
  Serial.println();
  if (WiFi.status() != WL_CONNECTED) { Serial.println("WiFi failed"); return; }
  Serial.print("WiFi connected: "); Serial.println(WiFi.localIP());
  snprintf(deviceId, sizeof(deviceId), "%s", WiFi.macAddress().c_str());
  Serial.print("Device ID: "); Serial.println(deviceId);
  registerDevice();
  BLEDevice::init("");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) { WiFi.reconnect(); delay(2000); return; }
  unsigned long now = millis();
  if (now - lastPoll > 5000) { checkMessages(); lastPoll = now; }
  if (now - lastHeartbeat > 15000) {
    char encDev[32]; urlEncode(deviceId, encDev, sizeof(encDev));
    char query[128]; snprintf(query, sizeof(query), "action=heartbeat&device=%s", encDev);
    char resp[128]; httpGet(query, resp, sizeof(resp));
    lastHeartbeat = now;
  }
}