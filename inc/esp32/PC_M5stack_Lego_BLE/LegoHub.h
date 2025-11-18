#ifndef LEGO_HUB_H
#define LEGO_HUB_H

#include <Arduino.h>

/*
  LegoHub.h

  BLE-client richting LEGO Powered Up / Duplo / Train hub (LWP3).

  API:
    void LegoHub_Init(const char* hubMac);
    void LegoHub_Update();              // reconnect & background logic
    bool LegoHub_IsConnected();
    void LegoHub_SetMotorPower(int8_t); // voorbeeld: motor op port 0
*/

void LegoHub_Init(const char* hubMac);
void LegoHub_Update();

bool LegoHub_IsConnected();
void LegoHub_SetMotorPower(int8_t power);

#endif
