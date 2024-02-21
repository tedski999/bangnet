#include "ntp.hpp"
#include "oled.hpp"
#include "ota.hpp"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_log.h>

#define TAG "ESP32resso"

#define WIFI_SSID "Swifty House Wifi"
#define WIFI_PSWD "Cheeseandham7"

#define BUTTON_PIN 0
#define BUTTON_TIME_MSEC 100

static void on_timeout(TimerHandle_t timer)
{
	if (digitalRead(BUTTON_PIN) == 0)
		bnet_oled_show();
}

extern "C" void app_main()
{
	ESP_LOGI(TAG, "Initialising board...");
	initArduino();
	randomSeed(analogRead(0));

	ESP_LOGI(TAG, "Configuring button handler...");
	pinMode(BUTTON_PIN, INPUT_PULLUP);
	TimerHandle_t timer = xTimerCreate("button_timer", BUTTON_TIME_MSEC / portTICK_PERIOD_MS, true, NULL, on_timeout);
	xTimerStart(timer, 0);

	ESP_LOGI(TAG, "Connecting to WiFi...");
	WiFi.begin(WIFI_SSID, WIFI_PSWD);
	while (WiFi.status() != WL_CONNECTED) {
		delay(100);
	}

	ESP_LOGI(TAG, "Starting application systems...");
	bnet_ntp_start();
	bnet_ota_start();
}
