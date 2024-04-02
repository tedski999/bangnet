// Code sources:
// - https://gist.github.com/Kaan2626/2728a8ff911c732426403f43383f8bb4

#include <Arduino.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_http_client.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/timers.h>

#include "driver/mcpwm.h"

#include <WiFi.h>

extern "C" {
#include <detools.h>
}

#define CLOUD_HOST "34.241.234.234"
#define CLOUD_PORT (8000)
#define CLOUD_ENDPOINT "/servo"
#define CLOUD_MIN_POLL_MSEC (30000)
#define CLOUD_MAX_POLL_MSEC (60000)

#define TAG "ESP32resso Servo Control"

// FreeRTOS task parameters
#define TASK_STACK_SIZE (8192)
#define TASK_PRIORITY (5)

// WiFi settings
#define WIFI_SSID "msungie"
#define WIFI_PSWD "hamandcheese"

// WiFiClient wifiClient;
// HttpClient httpClient = HttpClient(wifiClient, serverAddress, serverPort);

// int status = WL_IDLE_STATUS;

// Servo configuration
#define SERVO_MIN_PULSEWIDTH 544
#define SERVO_MAX_PULSEWIDTH 2400
#define SERVO_MAX_DEGREE 180
#define cameraServoPin 14
int cameraServoPosition = 0;


// void update_servo_position(void *arg) {
//   while(true) {
//     // Our servo's range is 0-180 degrees.
//     // I've tried giving it negative positions and positions above 180 and it
//     // will stop moving after that.

//     httpClient.get(serverPath);
//     int statusCode = httpClient.responseStatusCode();
//     if (statusCode != 200) {
//       Serial.println("GET request to " + serverAddress + ":" + serverPort + serverPath + " failed");
//       Serial.println("Failed with status code " + statusCode);
//       delay(1000);
//       return;
//     }
    
//     // read response from server and ensure it's within the bounds of [0, 180]
//     cameraServoPosition = httpClient.responseBody().toInt();
//     cameraServoPosition = max(cameraServoPosition, 0);
//     cameraServoPosition = min(cameraServoPosition, 180);

//     Serial.print("Setting servo position to: ");
//     Serial.println(cameraServoPosition);
//     cameraServo.write(cameraServoPosition);

//     Serial.println("Waiting 5 seconds before next movement...");
//     delay(5000);
//   }
// }

static void initializeServo() {
    printf("initializing mcpwm servo control gpio\n");
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, cameraServoPin);    //Set GPIO 18 as PWM0A, to which servo is connected

    printf("Configuring Initial Parameters of mcpwm\n");
    mcpwm_config_t pwm_config;
    pwm_config.frequency = 50;    //frequency = 50Hz, i.e. for every servo motor time period should be 20ms
    pwm_config.cmpr_a = 0;    //duty cycle of PWMxA = 0
    pwm_config.cmpr_b = 0;    //duty cycle of PWMxb = 0
    pwm_config.counter_mode = MCPWM_UP_COUNTER;
    pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);    //Configure PWM0A & PWM0B with above settings
}

static uint32_t calculatePulseLength(uint32_t degree_of_rotation) {
    uint32_t cal_pulsewidth = 0;
    cal_pulsewidth = (SERVO_MIN_PULSEWIDTH + (((SERVO_MAX_PULSEWIDTH - SERVO_MIN_PULSEWIDTH) * (degree_of_rotation)) / (SERVO_MAX_DEGREE)));
    return cal_pulsewidth;
}

void moveServo(void *arg) {
    uint32_t angle;
    initializeServo();

	  uint32_t currentAngle = 0;
    while (true) {
      if (cameraServoPosition != currentAngle) {
        angle = calculatePulseLength(cameraServoPosition);
        mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, angle);
        currentAngle = cameraServoPosition;
      }

		  vTaskDelay(10);
    }
}

int getAngleFromServer() {
  // This code is based on the ota.cpp code from the sensor directory
  esp_http_client_config_t http_config = {};
  esp_http_client_handle_t http_handle;
  esp_err_t err;
  int64_t content_length;
  char read_buf[16];
	int read_len;

  http_config.host = CLOUD_HOST;
	http_config.port = CLOUD_PORT;
	http_config.path = CLOUD_ENDPOINT;

  int status_code;
	if (!(http_handle = esp_http_client_init(&http_config))) {
		ESP_LOGE(TAG, "Unable to create HTTP client");
		return -1;
	}

	if ((err = esp_http_client_open(http_handle, 0)) != ESP_OK) {
		ESP_LOGE(TAG, "Unable to open HTTP connection");
		esp_http_client_cleanup(http_handle);
		return -1;
	}

	if ((content_length = esp_http_client_fetch_headers(http_handle)) < 0) {
		ESP_LOGE(TAG, "Unable to read HTTP headers");
		esp_http_client_cleanup(http_handle);
		return -1;
	}

	if ((status_code = esp_http_client_get_status_code(http_handle)) != 200) {
		ESP_LOGE(TAG, "Received non-200 status code: %d", status_code);
		esp_http_client_cleanup(http_handle);
		return -1;
	}

 
  read_len = esp_http_client_read(http_handle, read_buf, sizeof read_buf);
  esp_http_client_cleanup(http_handle);

  int parsedValue = atoi(read_buf);
  printf("value from server: %d\n", parsedValue);
  return parsedValue;
}

void updateServoPosition(void *arg) {
  printf("Start of http request task\n");
  while (true) {
    int result = getAngleFromServer();
    if (result < 0) {
      printf("An error occurred when fetching the intended servo position from the server, defaulting to 0\n");
      cameraServoPosition = 0;
    }
    else {
      printf("Setting servo position to %d\n", cameraServoPosition);
      cameraServoPosition = result;
    }

    vTaskDelay(1000);
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
  snprintf(path, sizeof path, "/update?version=servo.%llX", version);
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
    printf("Started app_main\n");

    initArduino();
    WiFi.begin(WIFI_SSID, WIFI_PSWD);
    for (int i = 0; WiFi.status() != WL_CONNECTED && i < 30; i++)
        delay(1000);
    if (WiFi.status() != WL_CONNECTED) {
        ESP_LOGE("$MAIN", "Failed to connect to WiFi, rebooting...");
        esp_restart();
    }

    // xTaskCreate Parameters
    // ======================
    // TaskFunction_t pvTaskCode,
    // const char * const pcName,
    // const configSTACK_DEPTH_TYPE uxStackDepth,
    // void *pvParameters,
    // UBaseType_t uxPriority,
    // TaskHandle_t *pxCreatedTask
    xTaskCreate(moveServo, "moveServo", TASK_STACK_SIZE, NULL, TASK_PRIORITY, NULL);
    xTaskCreate(updateServoPosition, "updateServoPosition", TASK_STACK_SIZE, NULL, TASK_PRIORITY, NULL);
    xTaskCreate(ota_task, "ota_task", TASK_STACK_SIZE, NULL, TASK_PRIORITY, NULL);
  }
