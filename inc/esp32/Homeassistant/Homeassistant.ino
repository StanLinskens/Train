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

// Post a simple message back to server (device -> web)
void postResponse(const String &msg) {
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

// --- LED setup (RGB) ---
const int PIN_R = 23; // red
const int PIN_G = 22; // green
const int PIN_B = 21; // blue

void setupLEDs() {
  // Ensure the pins are configured as outputs first
  pinMode(PIN_R, OUTPUT);
  pinMode(PIN_G, OUTPUT);
  pinMode(PIN_B, OUTPUT);

  // initialize outputs to off
  analogWrite(PIN_R, 0);
  analogWrite(PIN_G, 0);
  analogWrite(PIN_B, 0);
}

// Set RGB color with 0-255 values
void setColor(uint8_t r, uint8_t g, uint8_t b) {
  // If the board's analogWrite range is different, scale accordingly.
  analogWrite(PIN_R, r);
  analogWrite(PIN_G, g);
  analogWrite(PIN_B, b);
  Serial.printf("Set color R=%u G=%u B=%u\n", r, g, b);
}

// Helper to send structured status updates back to server
void sendStatus(const String &statusType, const String &message, int progress = -1) {
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
      sendStatus("received", "order received");

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
          k.trim(); v.trim();
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

      if (action == "led") {
        // find r,g,b values (0-255), default 0
        int r = 0, g = 0, b = 0;
        for (int i = 0; i < pairCount; i++) {
          if (keys[i] == "r") r = vals[i].toInt();
          if (keys[i] == "g") g = vals[i].toInt();
          if (keys[i] == "b") b = vals[i].toInt();
        }
        // send step update: setting color
        sendStatus("step", "setting_color", 10);
        setColor((uint8_t)r, (uint8_t)g, (uint8_t)b);
        // simulate progress with several updates
        for (int p = 20; p <= 100; p += 20) {
          delay(150);
          sendStatus("progress", "setting_color", p);
        }
        sendStatus("done", "color_set");
        // final execution confirmation
        postResponse(String("ExecutedOrder|action=led;r=") + String(r) + ";g=" + String(g) + ";b=" + String(b));
      } else if (action == "move") {
        // placeholder: simulate movement with realtime updates
        sendStatus("step", "starting_move", 0);
        for (int p = 0; p <= 100; p += 25) {
          delay(400);
          sendStatus("progress", "moving", p);
        }
        sendStatus("done", "move_complete");
        postResponse(String("ExecutedOrder|action=move"));
      } else {
        // unknown order: record and ack
        sendStatus("error", "unknown_action");
        postResponse(String("ExecutedOrder|unknown_action"));
      }
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

  // initialize LEDs
  setupLEDs();

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