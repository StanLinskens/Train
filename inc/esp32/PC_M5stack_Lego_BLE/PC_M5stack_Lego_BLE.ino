#include <Arduino.h>
#include "BleUart.h"
#include "LegoHub.h"

// 1) MAC-adres van de LEGO trein-hub
//    → aanpassen aan jouw hub (via BLE-scanner/app gevonden)
const char* TRAIN_HUB_MAC = "9C:9A:C0:1B:C0:EA";   // TODO: aanpassen

// 2) Command-handler: wat doen we met berichten van pc/telefoon?
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
  }
  else if (cmd.equalsIgnoreCase("STOP")) {
    LegoHub_SetMotorPower(0);
    BleUart_Send("ACK:STOP");
  }
  else {
    BleUart_Send("ERR:UNKNOWN_CMD");
  }
}

void MoveTrain(int motorPower) {
  Serial.println("[APP]moving...");


  LegoHub_SetMotorPower(motorPower);
  delay(1000);
  LegoHub_SetMotorPower(0);
  BleUart_Send("ACK: moved");

}


void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("=== Main_LegoBridge ===");

  // BLE UART server richting pc/telefoon
  BleUart_Init("M5 LEGO Bridge");

  // LEGO hub client (mac-adres wordt hier doorgegeven)
  LegoHub_Init(TRAIN_HUB_MAC);
}

void loop() {
  // 1) LEGO hub verbindingslogica (probeert reconnects)
  LegoHub_Update();

  // 2) BLE UART logica (hier kun je later nog extra dingen in stoppen)
  BleUart_Update();

  // 3) Berichten ophalen en verwerken
  if (BleUart_HasNewMessage()) {
    String msg = BleUart_GetMessage();
    handleCommand(msg);
  }

  delay(10);
}
