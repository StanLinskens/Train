#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// Pins
#define TFT_CS     10
#define TFT_DC     8
#define TFT_RST    9
#define TFT_MOSI   7
#define TFT_SCLK   6
#define TFT_LED    2

// Force software SPI so we know it's on 6/7:
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

void setup() {
  Serial.begin(9600);
  // Backlight on
  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, HIGH);

  // Try each init type if first fails
  tft.initR(INITR_BLACKTAB);
  // tft.initR(INITR_GREENTAB);
  // tft.initR(INITR_REDTAB);

  tft.fillScreen(ST77XX_BLUE);
  delay(500);
  tft.fillScreen(ST77XX_RED);
  delay(500);
  tft.fillScreen(ST77XX_GREEN);
  delay(500);

  // Final message
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(false);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(10, 30);
  tft.println("TFT OK!");
}

void loop() {
  // nothing here
}
