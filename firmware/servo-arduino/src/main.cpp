#include <Arduino.h>
#include <WiFi.h>
#include <ESP32Servo.h>


// WiFi settings
// TODO(cian): make this more general or read these from another file
const char* ssid = ":house_with_garden:";
const char* password = "bluecow6";

// my pc, for now
// TODO(cian): set this to the EC2 instance's IP address once image uploading is possible
String serverAddress = "192.168.30.41";
int serverPort = 8080;
String serverPath = "/";

WiFiClient client;
int status = WL_IDLE_STATUS;

// I just picked a random pin that seemed to be free, there wasn't any special method for picking this
#define cameraServoPin 14
Servo cameraServo;
int cameraServoPosition = 0;

void setup() {
  Serial.begin(9600);
  Serial.print("Attempting to connect to network called: ");
  Serial.println(ssid); 
  WiFi.begin(ssid, password);
  while ( WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(100); // wait 100ms
  }
  Serial.println();


  // print out network details to the serial console
  Serial.print("Connected to SSID: ");
  Serial.println(WiFi.SSID());

  Serial.print("Current IP Address is: ");
  Serial.println(WiFi.localIP());

  Serial.print("Configuring servo driver to be on pin: ");
  Serial.println(cameraServoPin);
  cameraServo.attach(cameraServoPin);

  Serial.println();  // newline
}

void loop() {
  // Our servo's range is 0-180 degrees.
  // I've tried giving it negative positions and positions above 180 and it
  // will stop moving after that.

  // TODO(cian): fetch value from server and update `cameraServoPosition` with it
  cameraServoPosition = (cameraServoPosition + 10) % 180;

  Serial.print("Setting servo position to: ");
  Serial.println(cameraServoPosition);
  cameraServo.write(cameraServoPosition);

  Serial.println("Waiting 1 second before next movement...");
  delay(1000);
}