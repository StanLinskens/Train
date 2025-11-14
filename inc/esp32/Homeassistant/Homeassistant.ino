#include <WiFi.h>
#include <HTTPClient.h>

// WiFi credentials
const char* ssid = "MMS";
const char* password = "1M2a3r4l5i6n7t8t9";

// PHP endpoint
const char* serverName = "http://stan.1pc.nl/Train/inc/php/data.php";

String deviceId;

String urlEncode(const String &str) {
  String encoded = "";
  for (size_t i = 0; i < str.length(); i++) {
    char c = str[i];
    if (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('0' <= c && c <= '9') || c=='-' || c=='_' || c=='.' || c=='~') {
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

void postResponse(const String &msg) {
  HTTPClient http;
  String u = String(serverName) + "?action=post_response&device=" + urlEncode(deviceId) + "&msg=" + urlEncode(msg);
  http.begin(u);
  int code = http.GET();
  if (code > 0) {
    Serial.println("Posted response: " + http.getString());
  } else {
    Serial.println("Post response error: " + String(code));
  }
  http.end();
}

void checkMessages() {
  HTTPClient http;
  String u = String(serverName) + "?action=get_messages&device=" + urlEncode(deviceId);
  http.begin(u);
  int code = http.GET();
  if (code > 0) {
    String payload = http.getString();
    if (payload.length() > 0) {
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
        if (line.length() > 0) {
          Serial.println("CMD: " + line);
          // parse order messages: format ORDER|k=v;k2=v2
          if (line.startsWith("ORDER|")) {
            String body = line.substring(6);
            // split by ';'
            int idx = 0;
            // simple container for parsed pairs
            String ack = "ACK_ORDER|";
            while (idx < body.length()) {
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
                k.trim(); v.trim();
                Serial.println("  -> " + k + " = " + v);
                ack += k + "=" + v + ";";
              }
            }
            // simulate executing the order: here you'd control motors, switches, ...
            Serial.println("Executing order...");
            // send a confirmation back to server
            postResponse(String("ExecutedOrder|") + ack);
          } else {
            // plain text command: handle or echo
            String resp = String("Executed: ") + line;
            postResponse(resp);
          }
        }
      }
    }
  } else {
    Serial.println("get_messages HTTP error: " + String(code));
  }
  http.end();
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
  if (now - lastPoll > 5000) { // poll every 5 seconds
    checkMessages();
    lastPoll = now;
  }
  if (now - lastHeartbeat > 15000) { // heartbeat every 15s
    HTTPClient http;
    String u = String(serverName) + "?action=heartbeat&device=" + urlEncode(deviceId);
    http.begin(u);
    http.GET();
    http.end();
    lastHeartbeat = now;
  }
}