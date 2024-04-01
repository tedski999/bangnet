#include <LoRa.h>

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

#define OP_RESET 0x00
#define OP_ACK 0x01

byte LoRaBuffer[256];
byte img_buffer[1024];
int img_buffer_offset = 0;
long receievedTimer = 0;

boolean ack = false;
boolean shouldSendAck = false;

void LoRa_rxMode(){
  //LoRa.disableInvertIQ();                // active invert I and Q signals
  LoRa.receive();                       // set receive mode
  Serial.println("rx mode");
}

void LoRa_txMode(){
  LoRa.idle();                          // set standby mode
  //LoRa.enableInvertIQ();               // normal mode
  Serial.println("tx mode");
}

void onTxDone() {
    Serial.println("tx done");
    LoRa_rxMode();
}

void sendOperator(byte op) {
    LoRa_txMode();
    LoRa.beginPacket();
    LoRa.write(op);
    LoRa.endPacket();
    LoRa_rxMode();
    Serial.printf("Sent op %d\n", op);
}

void waitForAck() {
    long timeout = millis() + 5000;
    ack = false;
    while (!ack) {
        if (timeout < millis())  {
            Serial.println("err: ACK timeout");;
            break;
        }
    }
    //todo handle timeout errors
}

void ackReceived() {
    ack = true;
}

void sendImage(const byte *img_data) {
    sendOperator(OP_RESET);
    //assumes img_data is a multiple of 256
    for (int i = 0; i < 1024; i+=256) {
        waitForAck();
        LoRa_txMode();
        LoRa.beginPacket();
        LoRa.write(&img_data[i], 256);
        LoRa.endPacket();
        LoRa_rxMode();
    }
}

void resetImageBuffer() {
  img_buffer_offset = 0;
  for (int i = 0; i < 1024; i++) img_buffer[i] = 0x00;
}

void onReceive(int packetSize) {
    while (LoRa.available()) {
        LoRa.readBytes(LoRaBuffer, packetSize);
    }
    Serial.printf("received a packet of size %d\n", packetSize);
    receievedTimer = millis();

    //operator
    if (packetSize == 1) {
        switch (LoRaBuffer[0]) {
            case OP_RESET:
                resetImageBuffer();
                shouldSendAck = true;
                break;
            case OP_ACK:
                ackReceived();
                break;
            default:
                break;
        }
    } else { //data being sent
        for (int i = img_buffer_offset; i < img_buffer_offset + 256 && i < 1024; i++) {
            img_buffer[i] = LoRaBuffer[i-img_buffer_offset];
        }
        img_buffer_offset += 256;
        shouldSendAck = true;
    }
}

void testInterrupt() {
    Serial.println("ISR received");
}

void LoRaUpdate() {
    if (shouldSendAck) {
        shouldSendAck = false;
        sendOperator(OP_ACK);
    }
}

void displayImage(SSD1306Wire *display) {
    display->clear();
    display->drawFastImage(0, 0, 128, 64, img_buffer);
    display->display();
}

void LoRaSetup() {
    attachInterrupt(DIO0, testInterrupt, RISING);
    SPI.begin(SCK, MISO, MOSI, SS);
    LoRa.setPins(SS, RST, DIO0);

    if (!LoRa.begin(BAND)) {
        Serial.println("Starting LoRa failed!");
        while (1);
    }

    LoRa.setSpreadingFactor(12); //6-12
    LoRa.setSignalBandwidth(125E3);
    LoRa.setCodingRate4(8);
    //LoRa.setGain(6); //0-6

    //pinMode(SS, OUTPUT);
    //pinMode(RST, OUTPUT);
    //pinMode(DIO0, INPUT);

    LoRa.onReceive(onReceive);
    LoRa.onTxDone(onTxDone);
    LoRa_rxMode();
}