#include <WiFi.h>
#include <HTTPClient.h>
#include <Arduino.h>
#include "BleUart.h"
#include "LegoHub.h"
#include <M5Unified.h>

// --- WiFi / server (from Homeassistant.ino) ---
// WiFi credentials
const char* ssid = "Stan_moto";
const char* password = "Stan1203";

// PHP endpoint
const char* serverName = "http://stan.1pc.nl/Train/inc/php/data.php";

String deviceId;

String urlEncode(const String& s) {
	String encoded = "";
	char c;

	for (int i = 0; i < s.length(); i++) {
		c = s.charAt(i);

		// Allowed characters (RFC 3986)
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {

			encoded += c;

		} else {
			// Percent-encode everything else
			char hex[4];
			sprintf(hex, "%%%02X", (unsigned char)c);
			encoded += hex;
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
void checkMessages();  // forward

// --- LED setup (RGB) from Homeassistant.ino ---
const int PIN_R = 23;  // red
const int PIN_G = 22;  // green
const int PIN_B = 21;  // blue

void setupLEDs() {
	pinMode(PIN_R, OUTPUT);
	pinMode(PIN_G, OUTPUT);
	pinMode(PIN_B, OUTPUT);
	analogWrite(PIN_R, 0);
	analogWrite(PIN_G, 0);
	analogWrite(PIN_B, 0);
}

void setColor(uint8_t r, uint8_t g, uint8_t b) {
	analogWrite(PIN_R, r);
	analogWrite(PIN_G, g);
	analogWrite(PIN_B, b);
	Serial.printf("Set color R=%u G=%u B=%u\n", r, g, b);
}

// --- LEGO M5 / BLE logic (from PC_M5stack_Lego_BLE.ino) ---
// Replace with your train hub MAC (same as in PC sketch)
const char* TRAIN_HUB_MAC = "9C:9A:C0:1B:C0:EA";  // adjust if needed

int counter = 0;

// Draws the base UI
void drawUI() {
	M5.Display.clear();
	M5.Display.setCursor(10, 10);
	M5.Display.setTextSize(3);
	M5.Display.setTextColor(WHITE);

	M5.Display.println("lego ble hub");

	M5.Display.setTextSize(2);
	M5.Display.println("-----------------------");
	M5.Display.println("A = forward");
	M5.Display.println("B = backward");
	M5.Display.println("C = connect bluetooth");
	M5.Display.println("(M5 LEGO Bridge)");

	M5.Display.println("-----------------------");
}

// Function to move the train (from PC sketch)
void MoveTrain(int motorPower) {
	Serial.println("[APP]moving...");

	LegoHub_SetMotorPower(motorPower);
	delay(1000);
	LegoHub_SetMotorPower(0);
	BleUart_Send("ACK: moved");
}

// 2) Command-handler: what to do with messages from pc/phone
void handleCommand(const String& cmd) {
	Serial.print("[APP] handleCommand: ");
	Serial.println(cmd);

	if (cmd.equalsIgnoreCase("FORWARD")) {
		MoveTrain(100);
		return;
	}
	if (cmd.equalsIgnoreCase("BACKWARD")) {
		MoveTrain(-100);
		return;
	}

	if (!LegoHub_IsConnected()) {
		BleUart_Send("ERR:LEGO_NOT_CONNECTED");
		return;
	}

	if (cmd.startsWith("M")) {
		int value = cmd.substring(1).toInt();
		LegoHub_SetMotorPower((int8_t)value);
		BleUart_Send("ACK:MOTOR:" + String(value));
	} else if (cmd.equalsIgnoreCase("STOP")) {
		LegoHub_SetMotorPower(0);
		BleUart_Send("ACK:STOP");
	} else {
		BleUart_Send("ERR:UNKNOWN_CMD");
	}
}

// --- Combined checkMessages implementation ---
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
					k.trim();
					v.trim();
					keys[pairCount] = k;
					vals[pairCount] = v;
					pairCount++;
					Serial.println("  -> " + k + " = " + v);
				}
			}

			// execute known action types
			String action = "";
			for (int i = 0; i < pairCount; i++) {
				if (keys[i] == "action") action = vals[i];
			}

			if (action == "led") {
				int r = 0, g = 0, b = 0;
				for (int i = 0; i < pairCount; i++) {
					if (keys[i] == "r") r = vals[i].toInt();
					if (keys[i] == "g") g = vals[i].toInt();
					if (keys[i] == "b") b = vals[i].toInt();
				}
				sendStatus("step", "setting_color", 10);
				setColor((uint8_t)r, (uint8_t)g, (uint8_t)b);
				for (int p = 20; p <= 100; p += 20) {
					delay(150);
					sendStatus("progress", "setting_color", p);
				}
				sendStatus("done", "color_set");
				postResponse(String("ExecutedOrder|action=led;r=") + String(r) + ";g=" + String(g) + ";b=" + String(b));
			} else if (action == "move") {
				// If move contains a 'power' or 'p' parameter, use it; else default to 100
				int power = 100;
				for (int i = 0; i < pairCount; i++) {
					if (keys[i] == "power" || keys[i] == "p") power = vals[i].toInt();
				}
				// Use LEGO control to move train
				sendStatus("step", "starting_move", 0);
				MoveTrain(power);
				for (int p = 0; p <= 100; p += 25) {
					delay(100);
					sendStatus("progress", "moving", p);
				}
				sendStatus("done", "move_complete");
				postResponse(String("ExecutedOrder|action=move;power=") + String(power));
			} else {
				sendStatus("error", "unknown_action");
				postResponse(String("ExecutedOrder|unknown_action"));
			}
		} else {
			// plain text command: handle via BLE/PC command handler if applicable
			// prefer BLE path if message starts with known prefixes
			if (line.equalsIgnoreCase("FORWARD") || line.equalsIgnoreCase("BACKWARD") || line.startsWith("M") || line.equalsIgnoreCase("STOP")) {
				handleCommand(line);
			} else {
				String resp = String("Executed: ") + line;
				postResponse(resp);
			}
		}
	}
}

// --- Setup / Loop ---
void setup() {
	Serial.begin(115200);
	delay(1000);
	Serial.println("Starting WebCon combined client");

	// initialize LEDs
	setupLEDs();

	// BLE UART server (PC/phone)
	BleUart_Init("M5 LEGO Bridge");

	// LEGO hub client
	LegoHub_Init(TRAIN_HUB_MAC);

	auto cfg = M5.config();
	M5.begin(cfg);

	drawUI();

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
		// continue; BLE/local features still available
	} else {
		Serial.println("WiFi connected: " + WiFi.localIP().toString());
		deviceId = WiFi.macAddress();
		Serial.println("Device ID: " + deviceId);
		registerDevice();
	}
}

unsigned long lastPoll = 0;
unsigned long lastHeartbeat = 0;

void loop() {
	// LEGO & BLE maintenance
	LegoHub_Update();
	BleUart_Update();

	// process BLE messages from phone/pc
	if (BleUart_HasNewMessage()) {
		String msg = BleUart_GetMessage();
		handleCommand(msg);
	}

	M5.update();

	// Buttons on M5
	if (M5.BtnA.wasPressed()) {
		MoveTrain(50);
		return;
	}
	if (M5.BtnB.wasPressed()) {
		MoveTrain(-50);
		return;
	}
	if (M5.BtnC.wasPressed()) {
		BleUart_Init("M5 LEGO Bridge");
		LegoHub_Init(TRAIN_HUB_MAC);
	}

	// WiFi/server polling (if connected)
	if (WiFi.status() == WL_CONNECTED) {
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

	delay(10);
}
