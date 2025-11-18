#include "BleUart.h"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// UUIDs van Nordic UART Service (NUS)
static const char* UART_SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
static const char* UART_CHAR_UUID_RX = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";
static const char* UART_CHAR_UUID_TX = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";

static BLEServer*         pUartServer = nullptr;
static BLECharacteristic* pTxChar     = nullptr;

static bool   uartClientConnected = false;
static bool   newMessage          = false;
static String lastMessage         = "";

// ---------------- Callbacks ----------------

class UartServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    uartClientConnected = true;
    Serial.println("[UART] client connected");
  }
  void onDisconnect(BLEServer* pServer) override {
    uartClientConnected = false;
    Serial.println("[UART] client disconnected");
    pServer->getAdvertising()->start();
  }
};

class UartRxCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    // In deze BLE-lib geeft getValue() een Arduino String terug
    String rx = pCharacteristic->getValue();
    if (rx.length() == 0) return;

    lastMessage = rx;
    newMessage  = true;

    Serial.print("[UART] RX: ");
    Serial.println(lastMessage);
  }
};

// ---------------- Publieke API ----------------

void BleUart_Init(const char* deviceName) {
  Serial.println("[UART] Init...");

  BLEDevice::init(deviceName);

  pUartServer = BLEDevice::createServer();
  pUartServer->setCallbacks(new UartServerCallbacks());

  BLEService* pService = pUartServer->createService(UART_SERVICE_UUID);

  // TX: M5 → PC (NOTIFY)
  pTxChar = pService->createCharacteristic(
    UART_CHAR_UUID_TX,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pTxChar->addDescriptor(new BLE2902());

  // RX: PC → M5 (WRITE)
  BLECharacteristic* pRxChar = pService->createCharacteristic(
    UART_CHAR_UUID_RX,
    BLECharacteristic::PROPERTY_WRITE
  );
  pRxChar->setCallbacks(new UartRxCallbacks());

  pService->start();

  BLEAdvertising* pAdvertising = pUartServer->getAdvertising();
  pAdvertising->addServiceUUID(UART_SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  BLEDevice::startAdvertising();

  Serial.println("[UART] BLE server ready.");
}

void BleUart_Update() {
  // Momenteel geen achtergrondwerk nodig;
  // placeholder voor toekomstige uitbreidingen.
}

bool BleUart_HasNewMessage() {
  return newMessage;
}

String BleUart_GetMessage() {
  newMessage = false;
  return lastMessage;
}

void BleUart_Send(const String& text) {
  if (!uartClientConnected || pTxChar == nullptr) return;

  pTxChar->setValue(text.c_str());
  pTxChar->notify();

  Serial.print("[UART] TX: ");
  Serial.println(text);
}
