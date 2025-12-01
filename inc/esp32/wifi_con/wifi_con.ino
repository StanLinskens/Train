#include <M5Unified.h>
#include <WiFi.h>
#include <esp_now.h>
#include <HTTPClient.h>

// Wi-Fi credentials (needed for initial Wi-Fi setup for HTTP)
const char* wifi_ssid = "Stan_moto";
const char* wifi_pass = "Stan1203";

// Server URL
const char* serverUrl = "http://stan.1pc.nl/Train/inc/php/data.php";

// Define peer MAC address (replace with your ESP32 B MAC address)
uint8_t peerAddress[] = { 0xF4, 0x65, 0x0B, 0x33, 0x79, 0xF2 };

const String deviceName = "";

// Struct to send messages via ESP-NOW
typedef struct struct_message {
  char msg[250];
} struct_message;

struct_message outgoingMessage;
struct_message incomingMessage;

// Get a unique device name from its Wi-Fi MAC address
String getDeviceName() {
  uint8_t mac[6];
  WiFi.macAddress(mac);  // get device MAC
  char name[20];
  snprintf(name, sizeof(name), "ESP32_%02X%02X%02X", mac[3], mac[4], mac[5]);
  return String(name);
}

void onDataReceive(const esp_now_recv_info_t* recv_info, const uint8_t* data, int data_len) {
  memcpy(&incomingMessage, data, sizeof(incomingMessage));
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           recv_info->src_addr[0], recv_info->src_addr[1], recv_info->src_addr[2],
           recv_info->src_addr[3], recv_info->src_addr[4], recv_info->src_addr[5]);
  Serial.println("Received via ESP-NOW from: " + String(macStr));
  Serial.println("Message: " + String(incomingMessage.msg));

  // Send response to server
  sendResponseToServer(deviceName, String(incomingMessage.msg));
}

void setup() {
  M5.begin();
  Serial.begin(115200);

  // Connect to Wi-Fi for HTTP
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to Wi-Fi");

  String deviceName = getDeviceName();
  Serial.println("Device Name: " + deviceName);
  registerDeviceOnline(deviceName);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Register receive callback
  esp_now_register_recv_cb(onDataReceive);

  // Add peer
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, peerAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
}

unsigned long lastHeartbeat = 0;

void loop() {
  // Poll server for messages
  String messages = getMessagesFromServer(deviceName);
  if (messages.length() > 0) {
    Serial.println("Messages from server: " + messages);
    sendToPeer(messages);
  }

  // Send heartbeat every 30 seconds
  if (millis() - lastHeartbeat > 30000) {
    sendHeartbeat(deviceName);
    lastHeartbeat = millis();
  }

  delay(1000);
}

// Send message via ESP-NOW
void sendToPeer(String msg) {
  msg.toCharArray(outgoingMessage.msg, sizeof(outgoingMessage.msg));
  esp_err_t result = esp_now_send(peerAddress, (uint8_t*)&outgoingMessage, sizeof(outgoingMessage));
  if (result == ESP_OK) {
    Serial.println("Sent via ESP-NOW: " + msg);
  } else {
    Serial.println("Error sending via ESP-NOW");
  }
}

String getMessagesFromServer(String device) {
  if (WiFi.status() != WL_CONNECTED) return "";

  HTTPClient http;
  String url = String(serverUrl) + "?action=get_messages&device=" + device + "&format=json";

  http.begin(url);
  int code = http.GET();
  String payload = (code == 200 ? http.getString() : "");
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

// Register device online
void registerDeviceOnline(String device) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = String(serverUrl) + "?action=connect&device=" + device;

  http.begin(url);
  int code = http.GET();
  if (code == 200) {
    Serial.println("Device registered online: " + device);
  } else {
    Serial.println("Failed to register device: " + device);
  }
  http.end();
}

// Send heartbeat to server
void sendHeartbeat(String device) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = String(serverUrl) + "?action=heartbeat&device=" + device;

  http.begin(url);
  http.GET();  // ignore response
  http.end();
}