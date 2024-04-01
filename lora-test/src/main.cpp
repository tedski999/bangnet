// Code initially stolen from here while I got the dev env set up:
// https://gist.github.com/tlrobinson/dd8cf7d0638bdf49f64812a77f7a798c
// The code has since been modified a tad to add an incrementing counter.
#define IS_RECEIVER true

#include <Wire.h>        // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306Wire.h" // legacy: #include "SSD1306.h"
#include "frequency.h"
#include "LoRaComms.h"

#if !IS_RECEIVER
#include "image.h"
#endif
// TTGO LoRa32-OLED V1
#define OLED_SDA 4
#define OLED_SCL 15
#define OLED_RST 16

SSD1306Wire display(0x3c, OLED_SDA, OLED_SCL);
char buffer[64];

void setup() {
  Serial.begin(9600);
  delay(100);
  Serial.println("hello world");

  // reset the OLED:
  if (OLED_RST != NOT_A_PIN)
  {
    pinMode(OLED_RST, OUTPUT);
    digitalWrite(OLED_RST, LOW); // set GPIO16 low to reset OLED
    delay(50);
    digitalWrite(OLED_RST, HIGH); // while OLED is running, must set GPIO16 in high
  }
  display.init();
  //display.flipScreenVertically();
  //display.normalDisplay();
  //display.invertDisplay();

  LoRaSetup();

  #if IS_RECEIVER
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawStringf(0, 0, buffer, "LoRa Receiver");
  display.display();
  #else
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawStringf(0, 0, buffer, "LoRa Sender");
  display.display();
  #endif

  setupFrequency();
}


void loop() {
  
  #if IS_RECEIVER
  sampleFrequency();
  if (receievedTimer + 1000 < millis()) {
    displayFrequency(&display);
  } else {
    displayImage(&display);
  }
  LoRaUpdate();

  //LoRa.
  #else
  sendImage(img_data);

  //display.clear();
  //display.drawFastImage(0, 0, 128, 64, img_data);
  //display.display();

  delay(10000);
  //while(1);
  #endif
  
}
