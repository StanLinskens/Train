#ifndef BLE_UART_H
#define BLE_UART_H

#include <Arduino.h>

/*
  BleUart.h

  Eenvoudige UART-achtige BLE-server richting pc/telefoon.

  API:
    void BleUart_Init(const char* deviceName);
    void BleUart_Update();              // nu nog leeg, maar voor de vorm
    bool BleUart_HasNewMessage();
    String BleUart_GetMessage();        // reset new-flag
    void BleUart_Send(const String&);   // stuur tekst naar client (notify)
*/

void BleUart_Init(const char* deviceName);
void BleUart_Update();

bool   BleUart_HasNewMessage();
String BleUart_GetMessage();
void   BleUart_Send(const String& text);

#endif
