#include <ArduinoHttpClient.h>
#include <WiFi.h>
#include "esp_camera.h"

// WiFi settings
// TODO: make this more general or read these from another file
const char* ssid = ":house_with_garden:";
const char* password = "bluecow6";

// pinout for camera on ESP-EYE according to https://github.com/espressif/esp-who/blob/master/docs/en/Camera_connections.md
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      4
#define SIOD_GPIO_NUM     18
#define SIOC_GPIO_NUM     23
#define Y9_GPIO_NUM       36
#define Y8_GPIO_NUM       37
#define Y7_GPIO_NUM       38
#define Y6_GPIO_NUM       39
#define Y5_GPIO_NUM       35
#define Y4_GPIO_NUM       14
#define Y3_GPIO_NUM       13
#define Y2_GPIO_NUM       34
#define VSYNC_GPIO_NUM     5
#define HREF_GPIO_NUM     27
#define PCLK_GPIO_NUM     25

// my pc, for now
// TODO: set this to the EC2 instance's IP address once image uploading is possible
String serverAddress = "192.168.30.41";
int serverPort = 8080;
String serverPath = "/";

WiFiClient client;
int status = WL_IDLE_STATUS;

void setupCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    ESP.restart();
  }
}

void setup() {
  Serial.begin(9600);
  Serial.print("Attempting to connect to network called: ");
  Serial.println(ssid);                   // print the network name (SSID);
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

  setupCamera();
}


void loop() {
  Serial.println("Capturing image... ");
  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();
  if(!fb) {
    Serial.print("Camera capture failed");
    delay(1000);
    ESP.restart();
  }
  Serial.print("Image captured");
  Serial.println();

  Serial.println("POSTing image (hopefully)...");


  // HTTP upload section adapted from https://randomnerdtutorials.com/esp32-cam-post-image-photo-server/
  if (client.connect(serverAddress.c_str(), serverPort)) {
    Serial.println("Connection successful!");    

    client.println("POST " + serverPath + " HTTP/1.1");
    client.println("Host: " + serverAddress);
    client.println("Content-Length: " + String(fb->len));
    client.println("Content-Type: multipart/form-data; boundary=RandomNerdTutorials");
    client.println();

    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    for (size_t n=0; n<fbLen; n=n+1024) {
      if (n+1024 < fbLen) {
        client.write(fbBuf, 1024);
        fbBuf += 1024;
      }
      else if (fbLen%1024>0) {
        size_t remainder = fbLen%1024;
        client.write(fbBuf, remainder);
      }
    }   
  }
  else {
    Serial.println("Couldn't connect to " + serverAddress);
  }

  // Now the image has been uploaded, free the frame buffer so it can be used again
  esp_camera_fb_return(fb);

  Serial.println("Waiting 5 seconds...");
  delay(5000);
}
