// Code sources:
// - https://gist.github.com/Kaan2626/2728a8ff911c732426403f43383f8bb4

#include <Arduino.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/timers.h>

#include "driver/mcpwm.h"

#include <WiFi.h>
#include <esp_http_client.h>

#define TAG "ESP32resso Servo Control"

// FreeRTOS task parameters
#define TASK_STACK_SIZE (8192)
#define TASK_PRIORITY (5)

// WiFi settings
// TODO(cian): make this more general or read these from another file
const char* ssid = ":house_with_garden:";
const char* password = "bluecow6";

// my pc, for now
// TODO(cian): set this to the EC2 instance's IP address once image uploading is possible
#define serverAddress "192.168.30.41"
#define serverPort 8080
#define serverPath "/servo";

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

  http_config.host = serverAddress;
	http_config.port = serverPort;
	http_config.path = serverPath;

  int status_code;
	if (!(http_handle = esp_http_client_init(&http_config))) {
		ESP_LOGE(TAG, "Unable to create HTTP client");
		return -1;
	}

	if ((err = esp_http_client_open(http_handle, 0)) != ESP_OK) {
		ESP_LOGE(TAG, "Unable to open HTTP connection");
		return -1;
	}

	if ((content_length = esp_http_client_fetch_headers(http_handle)) < 0) {
		ESP_LOGE(TAG, "Unable to read HTTP headers");
		return -1;
	}

	if ((status_code = esp_http_client_get_status_code(http_handle)) != 200) {
		ESP_LOGE(TAG, "Received non-200 status code: %d", status_code);
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

extern "C" void app_main(){
    printf("Started app_main\n");

    initArduino();
    WiFi.begin(ssid, password);
    while ( WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      delay(100); // wait 100ms
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
  }
