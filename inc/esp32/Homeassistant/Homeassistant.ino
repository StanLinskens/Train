#include <WiFi.h>
#include <HTTPClient.h>
#include "BLEDevice.h"

// WiFi credentials
const char* ssid = "Stan_moto";
const char* password = "Stan1203";

// PHP endpoint
const char* serverName = "http://stan.1pc.nl/Train/inc/php/data.php";

String deviceId;

static BLEAddress trainAddress("AA:BB:CC:DD:EE:FF");  // ← change to real MAC
static BLEUUID serviceUUID("1234");
static BLEUUID charUUID("5678");

BLERemoteCharacteristic* trainChar = nullptr;
BLEClient* bleClient = nullptr;

String lastBleResponse = "";

bool connectToTrain() {
  if (bleClient == nullptr) {
    bleClient = BLEDevice::createClient();
  }
  if (bleClient->isConnected()) return true;

  Serial.println("Connecting to train...");
  if (!bleClient->connect(trainAddress)) {
    Serial.println("BLE connect failed");
    return false;
  }

  BLERemoteService* svc = bleClient->getService(serviceUUID);
  if (!svc) {
    Serial.println("BLE service not found");
    return false;
  }

  trainChar = svc->getCharacteristic(charUUID);
  if (!trainChar) {
    Serial.println("BLE characteristic not found");
    return false;
  }

  // Register notify using lambda (works in ESP32 Arduino 3.3.3+)
  trainChar->registerForNotify([](BLERemoteCharacteristic* c, uint8_t* data, size_t length, bool isNotify) {
    lastBleResponse = "";
    for (size_t i = 0; i < length; i++) {
      lastBleResponse += (char)data[i];
    }
    Serial.println("Received BLE notify: " + lastBleResponse);
  });
  Serial.println("BLE connected to Train Controller!");
  return true;
}

bool sendBLE(const String& message) {
  if (!connectToTrain()) return false;

  trainChar->writeValue(message.c_str());
  Serial.println("BLE sent: " + message);
  return true;
}

String getLastBLEResponse() {
  String temp = lastBleResponse;
  lastBleResponse = "";
  return temp;
}

String urlEncode(const String& str) {
  String encoded = "";
  for (size_t i = 0; i < str.length(); i++) {
    char c = str[i];
    if (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('0' <= c && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else if (c == ' ') {
      encoded += '+';
    } else {
      char buf[5];
      sprintf(buf, "%%%.2X", (unsigned char)c);
      encoded += buf;
    }
  }
  return encoded;
}

// Post a simple message back to server (device -> web)
void postResponse(const String& msg) {
  HTTPClient http;
  String u = String(serverName) + "?action=post_response&device=" + urlEncode(deviceId) + "&msg=" + urlEncode(msg);
  http.begin(u);
  int code = http.GET();
  if (code > 0) {
    String body = http.getString();
    Serial.println("Posted response: " + body);
  } else {
    Serial.println("Post response error: " + String(code));
  }
  http.end();
}

// Helper to send structured status updates back to server
void sendStatus(const String& statusType, const String& message, int progress = -1) {
  // Format: STATUS|type=<type>;msg=<message>;progress=<n>
  String payload = "STATUS|type=" + statusType + ";msg=" + message;
  if (progress >= 0) payload += ";progress=" + String(progress);
  postResponse(payload);
}

void registerDevice() {
  HTTPClient http;
  String u = String(serverName) + "?action=connect&device=" + urlEncode(deviceId);
  http.begin(u);
  int code = http.GET();
  if (code > 0) {
    String body = http.getString();
    Serial.println("Register response: " + body);
  } else {
    Serial.println("Register HTTP error: " + String(code));
  }
  http.end();
}

// Poll server for pending messages and process them
void checkMessages() {
  HTTPClient http;
  String u = String(serverName) + "?action=get_messages&device=" + urlEncode(deviceId);
  http.begin(u);
  int code = http.GET();
  if (code <= 0) {
    Serial.println("get_messages HTTP error: " + String(code));
    http.end();
    return;
  }

  String payload = http.getString();
  http.end();

  if (payload.length() == 0) return;
  Serial.println("--- Received commands ---");

  int start = 0;
  while (start < payload.length()) {
    int nl = payload.indexOf('\n', start);
    String line;
    if (nl == -1) {
      line = payload.substring(start);
      start = payload.length();
    } else {
      line = payload.substring(start, nl);
      start = nl + 1;
    }
    line.trim();
    if (line.length() == 0) continue;

    Serial.println("CMD: " + line);
    // parse order messages: format ORDER|k=v;k2=v2
    if (line.startsWith("ORDER|")) {
      // send initial ack

      String body = line.substring(6);
      // parse into keys/values
      const int MAX_PAIRS = 20;
      String keys[MAX_PAIRS];
      String vals[MAX_PAIRS];
      int pairCount = 0;
      int idx = 0;
      while (idx < body.length() && pairCount < MAX_PAIRS) {
        int sc = body.indexOf(';', idx);
        String pair;
        if (sc == -1) {
          pair = body.substring(idx);
          idx = body.length();
        } else {
          pair = body.substring(idx, sc);
          idx = sc + 1;
        }
        int eq = pair.indexOf('=');
        if (eq > 0) {
          String k = pair.substring(0, eq);
          String v = pair.substring(eq + 1);
          k.trim();
          v.trim();
          keys[pairCount] = k;
          vals[pairCount] = v;
          pairCount++;
          Serial.println("  -> " + k + " = " + v);
        }
      }

      // execute known action types
      // supported: action=led (set rgb), action=move (placeholder)
      String action = "";
      for (int i = 0; i < pairCount; i++) {
        if (keys[i] == "action") action = vals[i];
      }

      // Build the ORDER exactly as received so we can forward it to ESP32-B
      String orderToSend = line;  // e.g. "ORDER|action=led;r=255;g=0;b=0"

      // 1. Tell server we received it
      sendStatus("received", "order received");

      // 2. Send ORDER to the train (BLE)
      if (!sendBLE(orderToSend)) {
        sendStatus("error", "ble_send_failed");
        postResponse("ExecutedOrder|error=ble_send_failed");
        return;  // BLE not connected
      }

      Serial.println("Order forwarded to train via BLE");

      // 3. Wait for train response
      unsigned long timeout = millis() + 5000;
      String bleResponse = "";

      while (millis() < timeout) {
        bleResponse = getLastBLEResponse();  // We'll implement this function
        if (bleResponse.length() > 0) break;
        delay(50);
      }

      if (bleResponse.length() == 0) {
        Serial.println("Train did not respond.");
        sendStatus("error", "train_no_response");
        postResponse("ExecutedOrder|error=train_no_response");
        return;
      }

      // 4. Train confirmed done → forward result to server
      Serial.println("Train response: " + bleResponse);
      postResponse(bleResponse);
      sendStatus("done", "order_complete");

    } else {
      // plain text command: handle or echo
      String resp = String("Executed: ") + line;
      postResponse(resp);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting ESP32 chat client");

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
    Serial.println("WiFi failed to connect");
    return;
  }
  Serial.println("WiFi connected: " + WiFi.localIP().toString());

  deviceId = WiFi.macAddress();
  Serial.println("Device ID: " + deviceId);
  registerDevice();
}

unsigned long lastPoll = 0;
unsigned long lastHeartbeat = 0;

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, attempting reconnect...");
    WiFi.reconnect();
    delay(2000);
    return;
  }

  unsigned long now = millis();
  if (now - lastPoll > 5000) {  // poll every 5 seconds
    checkMessages();
    lastPoll = now;
  }
  if (now - lastHeartbeat > 15000) {  // heartbeat every 15s
    HTTPClient http;
    String u = String(serverName) + "?action=heartbeat&device=" + urlEncode(deviceId);
    http.begin(u);
    http.GET();
    http.end();
    lastHeartbeat = now;
  }
}