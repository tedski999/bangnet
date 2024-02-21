#include "button.hpp"
#include "ntp.hpp"
#include "oled.hpp"
#include "ota.hpp"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_log.h>

#define TAG "ESP32resso"

#define WIFI_SSID "Swifty House Wifi"
#define WIFI_PSWD "Cheeseandham7"

extern "C" void app_main()
{
	ESP_LOGI(TAG, "Initialising board...");
	initArduino();
	randomSeed(analogRead(0));
	bnet_button_start();

	// Try connect to AP for 10 seconds
	ESP_LOGI(TAG, "Connecting to WiFi...");
	WiFi.begin(WIFI_SSID, WIFI_PSWD);
	for (int i = 0; WiFi.status() != WL_CONNECTED && i < 10; i++)
		delay(1 * MSECS_TO_SEC);
	if (WiFi.status() != WL_CONNECTED)
		WiFi.mode(WIFI_OFF);

	// Start appropriate systems
	if (WiFi.status() == WL_STOPPED) {
		ESP_LOGI(TAG, "Unable to connect to WiFi, enabling peer-connection systems...");
	} else {
		ESP_LOGI(TAG, "Connected to WiFi, enabling direct-connection systems...");
		bnet_ntp_start();
		bnet_ota_start();
	}
}
