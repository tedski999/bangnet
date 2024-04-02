#include "Arduino.h"
#include "esp_camera.h"
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_http_client.h>
#include <ArduinoHttpClient.h>
#include <WiFi.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/timers.h>

extern "C" {
#include <detools.h>
}

#define CLOUD_HOST "34.241.234.234"
#define CLOUD_PORT (8000)
#define CLOUD_MIN_POLL_MSEC (30000)
#define CLOUD_MAX_POLL_MSEC (60000)

// FreeRTOS task parameters
#define TASK_STACK_SIZE (8192)
#define TASK_PRIORITY (5)

// WiFi settings
#define WIFI_SSID "msungie"
#define WIFI_PSWD "hamandcheese"

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

// API details for uploading the images to the EC2 instance
String serverAddress = "34.241.234.234";
int serverPort = 8000;
String serverPath = "/api/upload_image/";

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

  sensor_t *sensor = esp_camera_sensor_get();
  sensor->set_vflip(sensor, 1);

}

void setup() {
  Serial.begin(115200);
  Serial.print("Attempting to connect to network called: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PSWD);
  for (int i = 0; WiFi.status() != WL_CONNECTED && i < 30; i++)
    delay(1000);
  if (WiFi.status() != WL_CONNECTED) {
    ESP_LOGE("$MAIN", "Failed to connect to WiFi, rebooting...");
    esp_restart();
  }
  Serial.println();


  // print out network details to the serial console
  Serial.print("Connected to SSID: ");
  Serial.println(WiFi.SSID());

  Serial.print("Current IP Address is: ");
  Serial.println(WiFi.localIP());

  setupCamera();

  Serial.println();  // newline
}


void work() {
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

  Serial.println("POSTing image...");


  // HTTP upload section adapted from https://randomnerdtutorials.com/esp32-cam-post-image-photo-server/
  if (client.connect(serverAddress.c_str(), serverPort)) {
    Serial.println("Connection successful!");

    String head = "--ESP32ressoHTTPSeparator\r\nContent-Disposition: form-data; name=\"file\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--ESP32ressoHTTPSeparator--\r\n";

    uint32_t imageLen = fb->len;
    uint32_t extraLen = head.length() + tail.length();
    uint32_t totalLen = imageLen + extraLen;

    client.println("POST " + serverPath + " HTTP/1.1");
    client.println("Host: " + serverAddress);
    client.println("Content-Length: " + String(totalLen));
    client.println("Content-Type: multipart/form-data; boundary=ESP32ressoHTTPSeparator");
    client.println();
    client.print(head);

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

    client.print(tail);

    String getAll;
    String getBody;

    int timoutTimer = 10000;
    long startTimer = millis();
    boolean state = false;
    
    while ((startTimer + timoutTimer) > millis()) {
      Serial.print(".");
      delay(100);      
      while (client.available()) {
        char c = client.read();
        if (c == '\n') {
          if (getAll.length()==0) { state=true; }
          getAll = "";
        }
        else if (c != '\r') { getAll += String(c); }
        if (state==true) { getBody += String(c); }
        startTimer = millis();
      }
      if (getBody.length()>0) { break; }
    }
    Serial.println();
    client.stop();
    Serial.println(getBody);
  
  }
  else {
    Serial.println("Couldn't connect to " + serverAddress);
  }

  // Now the image has been uploaded, free the frame buffer so it can be used again
  esp_camera_fb_return(fb);

  Serial.println("Waiting 5 seconds before next picture...");
  delay(5000);
}

void task(void *arg) {
    while(true) {
        work();
    }
}

// Occasionally poll cloud for delta-encoded over-the-air firmware updates
static esp_ota_handle_t ota_handle;
static const esp_partition_t *ota_new_partition, *ota_old_partition;
static uintptr_t ota_seek_addr = 0;
static int ota_read(void *arg, uint8_t *buf, size_t len) { return esp_partition_read(ota_old_partition, ota_seek_addr, buf, len); }
static int ota_seek(void *arg, int offset) { ota_seek_addr += offset; return 0; }
static int ota_write(void *arg, const uint8_t *buf, size_t len) { return esp_ota_write(ota_handle, buf, len) || ota_seek(arg, len); }
static void ota_task(void *arg) {
  printf("Start of firmware update task\n");
  uint64_t version = * (uint64_t *) esp_app_get_description()->app_elf_sha256;
  while (true) {
    delay(random(CLOUD_MIN_POLL_MSEC, CLOUD_MAX_POLL_MSEC));

  char path[256];
  esp_err_t err;
  esp_http_client_config_t http_config = {};
  esp_http_client_handle_t http_handle;
  int64_t content_len = 0, read_len = 0;
  int status_code;
  struct detools_apply_patch_t patch;

  ESP_LOGD("$", "Querying OTA for version %llX...", version);
  snprintf(path, sizeof path, "/update?version=camera.%llX", version);
  http_config.host = CLOUD_HOST;
  http_config.port = CLOUD_PORT;
  http_config.path = path;
  if (!(http_handle = esp_http_client_init(&http_config))) {
    ESP_LOGE("$", "Unable to create HTTP client");
    continue;
  }
  if ((err = esp_http_client_open(http_handle, 0)) != ESP_OK) {
    ESP_LOGE("$", "Unable to open HTTP connection: %s (err %d)", esp_err_to_name(err), err);
    goto end1;
  }
  if ((content_len = esp_http_client_fetch_headers(http_handle)) < 0) {
    ESP_LOGE("$", "Unable to read HTTP headers");
    goto end1;
  }
  if ((status_code = esp_http_client_get_status_code(http_handle)) != 200) {
    ESP_LOGE("$", "Received non-200 status code: %d", status_code);
    goto end1;
  }
  if (!content_len) {
    ESP_LOGD("$", "No OTA for version %llX received", version);
    goto end1;
  }

  ESP_LOGI("$", "Updating firmware (%lld byte patch)...", content_len);

  ota_seek_addr = 0;
  if (!(ota_new_partition = esp_ota_get_next_update_partition(NULL))) {
    ESP_LOGE("$", "Unable to find valid OTA partition to use");
    goto end1;
  }
  if (!(ota_old_partition = esp_ota_get_running_partition())) {
    ESP_LOGE("$", "Unable to find current OTA partition in use");
    goto end1;
  }
  err = esp_ota_begin(ota_new_partition, OTA_SIZE_UNKNOWN, &ota_handle);
  if (err != ESP_OK) {
    ESP_LOGE("$", "Unable to start OTA: err %s (err %d)", esp_err_to_name(err), err);
    goto end1;
  }
  if ((err = detools_apply_patch_init(&patch, ota_read, ota_seek, content_len, ota_write, NULL))) {
    ESP_LOGE("$", "Unable to init OTA: err %d", err);
    goto end2;
  }

  while (read_len < content_len) {
    int len; char buf[512];
    if ((len = esp_http_client_read(http_handle, buf, sizeof buf)) < 0) {
      ESP_LOGE("$", "Unable to read HTTP data");
      goto end1;
    }
    read_len += len;
    ESP_LOGI("$", "Writing OTA chunk (%llu / %llu)...", read_len, content_len);
    err = detools_apply_patch_process(&patch, (uint8_t *) buf, len);
    if (err) {
      ESP_LOGE("$", "Unable to patch OTA chunk: err %d", err);
      goto end3;
    }
  }

end3:
  if ((err = detools_apply_patch_finalize(&patch)) < 0) {
    ESP_LOGE("$", "Unable to complete OTA patch: err %d", err);
  } else if (read_len >= content_len) {
    ESP_LOGW("$", "OTA patch complete.");
    if ((err = esp_ota_end(ota_handle)) != ESP_OK) {
      ESP_LOGE("$", "Unable to complete OTA: %s (err %d)", esp_err_to_name(err), err);
      goto end1;
    }
    if ((err = esp_ota_set_boot_partition(ota_new_partition)) != ESP_OK) {
      ESP_LOGE("$", "Unable to set OTA boot partition: %s (err %d)", esp_err_to_name(err), err);
      goto end1;
    }
    ESP_LOGW("$", "Rebooting into new firmware...");
    delay(5000);
    esp_restart();
  }
end2:
  if ((err = esp_ota_abort(ota_handle)) != ESP_OK)
    ESP_LOGE("$", "Unable to gracefully abort OTA: err %d", err);
end1:
  if ((err = esp_http_client_cleanup(http_handle)) != ESP_OK)
    ESP_LOGE("$", "Unable to gracefully cleanup HTTP client: %s (err %d)", esp_err_to_name(err), err);
  }
}

extern "C" void app_main(){
    initArduino();
    setup();

    xTaskCreate(task, "task", TASK_STACK_SIZE, NULL, TASK_PRIORITY, NULL);
    xTaskCreate(ota_task, "ota_task", TASK_STACK_SIZE, NULL, TASK_PRIORITY, NULL);
}
