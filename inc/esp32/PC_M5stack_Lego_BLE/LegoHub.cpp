#include "LegoHub.h"

#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEUtils.h>

// LEGO Wireless Protocol v3 (LWP3) UUIDs
static const char* LEGO_SERVICE_UUID = "00001623-1212-efde-1623-785feabcd123";
static const char* LEGO_CHAR_UUID    = "00001624-1212-efde-1623-785feabcd123";

// Opgeslagen MAC-adres (string)
static String hubMacString = "";

static BLEClient*               pLegoClient  = nullptr;
static BLERemoteService*        pLegoService = nullptr;
static BLERemoteCharacteristic* pLegoChar    = nullptr;

static bool legoConnected = false;

static unsigned long lastReconnectAttempt   = 0;
static const unsigned long RECONNECT_INTERVAL_MS = 5000;

// ---------------- interne helpers ----------------

static bool connectToLegoHub() {
  if (hubMacString.length() == 0) {
    Serial.println("[LEGO] No MAC configured!");
    return false;
  }

  Serial.print("[LEGO] Connecting to hub at ");
  Serial.println(hubMacString);

  if (pLegoClient == nullptr) {
    pLegoClient = BLEDevice::createClient();
  }

  if (!pLegoClient->connect(BLEAddress(hubMacString.c_str()))) {
    Serial.println("[LEGO] Failed to connect");
    legoConnected = false;
    return false;
  }

  Serial.println("[LEGO] Connected, discovering service...");
  pLegoService = pLegoClient->getService(BLEUUID(LEGO_SERVICE_UUID));
  if (pLegoService == nullptr) {
    Serial.println("[LEGO] Service not found");
    pLegoClient->disconnect();
    legoConnected = false;
    return false;
  }

  pLegoChar = pLegoService->getCharacteristic(BLEUUID(LEGO_CHAR_UUID));
  if (pLegoChar == nullptr) {
    Serial.println("[LEGO] Characteristic not found");
    pLegoClient->disconnect();
    legoConnected = false;
    return false;
  }

  Serial.println("[LEGO] Hub connection OK");
  legoConnected = true;
  return true;
}

// let op: geen const, Kolban writeValue verwacht uint8_t*
static bool legoSendCommand(uint8_t* data, size_t len) {
  if (!legoConnected || pLegoChar == nullptr) {
    Serial.println("[LEGO] legoSendCommand: not connected");
    return false;
  }
  pLegoChar->writeValue(data, len, false);
  return true;
}

// ---------------- Publieke API ----------------

void LegoHub_Init(const char* hubMac) {
  hubMacString = String(hubMac);
  Serial.print("[LEGO] Init with MAC: ");
  Serial.println(hubMacString);

  // BLEDevice::init() wordt al in BleUart_Init() gedaan in dit project.
  // Als je LegoHub los wilt gebruiken, moet je hier ook BLEDevice::init() aanroepen.

  // Eerste connectiepoging
  connectToLegoHub();
}

void LegoHub_Update() {
  if (!legoConnected) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > RECONNECT_INTERVAL_MS) {
      lastReconnectAttempt = now;
      Serial.println("[LEGO] Trying to reconnect...");
      connectToLegoHub();
    }
  }
}

bool LegoHub_IsConnected() {
  return legoConnected;
}

// Simpel voorbeeld: motorvermogen op port 0 zetten
void LegoHub_SetMotorPower(int8_t power) {
  if (power < -100) power = -100;
  if (power >  100) power =  100;

  uint8_t msg[8];
  msg[0] = 0x08;
  msg[1] = 0x00;
  msg[2] = 0x81;    // Port Output Command
  msg[3] = 0x00;    // Port ID (0 = voorbeeld)
  msg[4] = 0x01;    // execute immediately
  msg[5] = 0x51;    // WriteDirectModeData
  msg[6] = 0x00;    // mode 0 = power
  msg[7] = (uint8_t)power;

  Serial.print("[LEGO] motor power = ");
  Serial.println(power);

  legoSendCommand(msg, sizeof(msg));
}
