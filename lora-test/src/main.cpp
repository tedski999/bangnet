// Code initially stolen from here while I got the dev env set up:
// https://gist.github.com/tlrobinson/dd8cf7d0638bdf49f64812a77f7a798c
// The code has since been modified a tad to add an incrementing counter.
#define IS_RECEIVER false

#include <Wire.h>        // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306Wire.h" // legacy: #include "SSD1306.h"
#include <LoRa.h>

#if !IS_RECEIVER
#include "image.h"
#endif
// TTGO LoRa32-OLED V1
#define OLED_SDA 4
#define OLED_SCL 15
#define OLED_RST 16



//define the pins used by the LoRa transceiver module
#define SCK 5
#define MISO 19
#define MOSI 27
#define SS 18
#define RST 14
#define DIO0 26

//433E6 for Asia
//866E6 for Europe
//915E6 for North America
#define BAND 866E6

SSD1306Wire display(0x3c, OLED_SDA, OLED_SCL);

static char buffer[32];
#if IS_RECEIVER
byte LoRaBuffer[256];
byte img_buffer[1024];
int img_buffer_offset = 0;
#endif
int counter = 0;

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

  SPI.begin(SCK, MISO, MOSI, SS);
  LoRa.setPins(SS, RST, DIO0);

  if (!LoRa.begin(BAND)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }

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
}

#if IS_RECEIVER
void resetImageBuffer() {
  img_buffer_offset = 0;
}
#endif

void loop() {
  
  #if IS_RECEIVER
  int packet = LoRa.parsePacket();
  if (packet) {

    while (LoRa.available()) {
      LoRa.readBytes(LoRaBuffer, packet);
      if (packet == 1 && LoRaBuffer[0] == 0x00) {
        resetImageBuffer();
      } else {

        for (int i = img_buffer_offset; i < img_buffer_offset + 256 && i < 1024; i++) {
          img_buffer[i] = LoRaBuffer[i-img_buffer_offset];
        }
        img_buffer_offset += 256;

      }
    }
    display.clear();
    display.drawFastImage(0, 0, 128, 64, img_buffer);
    display.display();

    //int rssi = LoRa.packetRssi();
  }
  #else
  //Send LoRa packet to receiver
  LoRa.beginPacket();
  LoRa.write(0x00);
  LoRa.endPacket();


  //assumes img_data is a multiple of 256
  for (int i = 0; i < sizeof(img_data); i+=256) {
    delay(500);
    LoRa.beginPacket();
    LoRa.write(&img_data[i], 256);
    LoRa.endPacket();
  }

  display.clear();
  display.drawFastImage(0, 0, 128, 64, img_data);
  display.display();

  delay(10000);
  #endif
  
}
