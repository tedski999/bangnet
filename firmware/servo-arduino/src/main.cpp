#include <WiFi.h>

// WiFi settings
// TODO: make this more general or read these from another file
const char* ssid = ":house_with_garden:";
const char* password = "bluecow6";

// my pc, for now
// TODO: set this to the EC2 instance's IP address once image uploading is possible
String serverAddress = "192.168.30.41";
int serverPort = 8080;
String serverPath = "/";

WiFiClient client;
int status = WL_IDLE_STATUS;

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

  Serial.println()  // newline
}

void loop() {
  Serial.println("Waiting 5 seconds before next movement...");
  delay(5000);
}