#include "main.hpp"
#include <WiFi.h>
#include <esp_log.h>
#include <esp_ota_ops.h>

#define TAG "ESP32resso"

#define TASK_STACK_SIZE (8192)
#define TASK_PRIORITY (5)

static bnet_state state;

extern "C" void app_main()
{
	ESP_LOGW(TAG, "Initialising app...");
	initArduino();
	Serial.begin(115200);
	randomSeed(analogRead(0));
	state = (struct bnet_state) {
		.lock = xSemaphoreCreateMutex(),
		.lora = xSemaphoreCreateMutex(),
		.bang_queue = xQueueCreate(8, sizeof (bnet_packet_t)),
		.ping_queue = xQueueCreate(8, sizeof (bnet_packet_t)),
		.lota_queue = xQueueCreate(128, sizeof (bnet_packet_t)),
		.ota_queue = xQueueCreate(8, sizeof (bnet_packet_t)),
		.ntp_time_usec = 0,
		.esp_time_usec = 0,
		.is_connecting = true,
		.is_gateway = false,
		.version = esp_app_get_description()->app_elf_sha256[0], // TODO: 0:8,
		.lota_version = 0,
		.ota_progress = 0,
		.bangs_detected = 0,
	};
	xTaskCreate(bnet_oled, "oled", TASK_STACK_SIZE, &state, TASK_PRIORITY, NULL);
	xTaskCreate(bnet_bang, "bang", TASK_STACK_SIZE, &state, TASK_PRIORITY, NULL);
	xTaskCreate(bnet_ping, "ping", TASK_STACK_SIZE, &state, TASK_PRIORITY, NULL);
	xTaskCreate(bnet_ota, "ota", TASK_STACK_SIZE, &state, TASK_PRIORITY, NULL);

	ESP_LOGW(TAG, "Attempting to connecting to local WiFi AP...");
	if (ESP.getEfuseMac() == 0x5c9a66f23a08) WiFi.begin(WIFI_SSID, WIFI_PSWD); // TODO: hardcoded
	for (int i = 0; WiFi.status() != WL_CONNECTED && i < WIFI_ATTEMPT_SEC; i++)
		delay(1 * MSECS_TO_SEC);

	xSemaphoreTake(state.lock, portMAX_DELAY);
	state.is_connecting = false;
	state.is_gateway = (WiFi.status() == WL_CONNECTED);
	if (state.is_gateway) {
		ESP_LOGE(TAG, "WiFi connection established, performing as gateway sensor device...");
		xTaskCreate(bnet_time_gateway, "time_gateway", TASK_STACK_SIZE, &state, TASK_PRIORITY, NULL);
		xTaskCreate(bnet_lora_gateway, "lora_gateway", TASK_STACK_SIZE, &state, TASK_PRIORITY, NULL);
		xTaskCreate(bnet_bang_gateway, "bang_gateway", TASK_STACK_SIZE, &state, TASK_PRIORITY, NULL);
		xTaskCreate(bnet_ping_gateway, "ping_gateway", TASK_STACK_SIZE, &state, TASK_PRIORITY, NULL);
		xTaskCreate(bnet_sync_gateway, "sync_gateway", TASK_STACK_SIZE, &state, TASK_PRIORITY, NULL);
	} else {
		ESP_LOGE(TAG, "WiFi not connected, performing as remote sensor device...");
		WiFi.mode(WIFI_OFF);
		xTaskCreate(bnet_lora_remote, "lora_remote", TASK_STACK_SIZE, &state, TASK_PRIORITY, NULL);
		xTaskCreate(bnet_bang_remote, "bang_remote", TASK_STACK_SIZE, &state, TASK_PRIORITY, NULL);
		xTaskCreate(bnet_ping_remote, "ping_remote", TASK_STACK_SIZE, &state, TASK_PRIORITY, NULL);
	}
	xSemaphoreGive(state.lock);
}
