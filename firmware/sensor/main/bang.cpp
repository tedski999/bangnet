#include "main.hpp"
#include <LoRa.h>
#include <esp_log.h>
#include <esp_timer.h>

#define TAG "ESP32resso BANG"

#define POLL_MSEC (200)
#define COOLDOWN_MSEC (500)
#define THRESHOLD (9999)

static void stub_timer(TimerHandle_t timer) {}

void bnet_bang(void *raw_state)
{
	ESP_LOGW(TAG, "Starting bang task...");
	struct bnet_state *state = (struct bnet_state *) raw_state;

	ESP_LOGI(TAG, "Waiting for first time sync...");
	while (!state->ntp_time_usec)
		delay(1 * MSECS_TO_SEC);

	ESP_LOGW(TAG, "Time synchronised, starting bang sensor...");
	TimerHandle_t timer = xTimerCreate("mic_cooldown", COOLDOWN_MSEC / portTICK_PERIOD_MS, false, NULL, stub_timer);
	while (true) {
		// analogRead(MICROPHONE_PIN) > THRESHOLD) {
		if (!xTimerIsTimerActive(timer) && random(100) == 0) {
		// if (false) {
			uint64_t now_usec = esp_timer_get_time();

			ESP_LOGW(TAG, "Microphone bang threshold triggered!");
			xSemaphoreTake(state->lock, portMAX_DELAY);
			state->bangs_detected += 1;
			bnet_packet_t bang = { ESP.getEfuseMac() | 1, state->ntp_time_usec + now_usec - state->esp_time_usec }; // TODO: endianness
			xSemaphoreGive(state->lock);
			ESP_LOGI(TAG, "Sending bang item...");
			xQueueSendToBack(state->bang_queue, bang, portMAX_DELAY);
			xTimerReset(timer, portMAX_DELAY);
			ESP_LOGI(TAG, "Sent bang item.");
		}

		delay(POLL_MSEC);
	}
}

void bnet_bang_gateway(void *raw_state)
{
	ESP_LOGW(TAG, "Starting gateway bang task...");
	struct bnet_state *state = (struct bnet_state *) raw_state;

	while (true) {
		ESP_LOGI(TAG, "Waiting for bang item...");
		bnet_packet_t bang;
		xQueueReceive(state->bang_queue, bang, portMAX_DELAY);
		ESP_LOGI(TAG, "Got bang item.");

		ESP_LOGW(TAG, "Uploading bang...");
		uint64_t board_id = bang[0], time_usec = bang[1];
		ESP_LOGI(TAG, "TODO POST bang: %llu @ %lld usec", board_id, time_usec);
		ESP_LOGI(TAG, "Uploaded bang.");
	}
}

void bnet_bang_remote(void *raw_state)
{
	ESP_LOGW(TAG, "Starting remote bang task...");
	struct bnet_state *state = (struct bnet_state *) raw_state;

	while (true) {
		ESP_LOGI(TAG, "Waiting for bang item...");
		bnet_packet_t bang;
		xQueueReceive(state->bang_queue, bang, portMAX_DELAY);
		ESP_LOGI(TAG, "Got bang item.");

		ESP_LOGW(TAG, "Broadcasting bang...");
		xSemaphoreTake(state->lora, portMAX_DELAY);
		LoRa.idle();
		LoRa.beginPacket(true);
		LoRa.write((uint8_t *) bnet_header, sizeof bnet_header);
		LoRa.write((uint8_t *) bang, sizeof bang);
		LoRa.endPacket(true);
		LoRa.receive(sizeof (bnet_header_t) + sizeof (bnet_packet_t));
		xSemaphoreGive(state->lora);
		ESP_LOGI(TAG, "Broadcasted bang.");
	}
}
