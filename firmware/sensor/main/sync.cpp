#include "main.hpp"
#include <LoRa.h>
#include <esp_log.h>

#define TAG "ESP32resso SYNC"

#define SYNC_SEC 30

void bnet_sync_gateway(void *raw_state)
{
	ESP_LOGW(TAG, "Starting gateway sync task...");
	struct bnet_state *state = (struct bnet_state *) raw_state;

	ESP_LOGI(TAG, "Waiting for first time sync...");
	while (!state->ntp_time_usec)
		delay(1 * MSECS_TO_SEC);

	// TODO: cleanup
	ESP_LOGW(TAG, "Time synchronised, starting sync broadcasts...");
	while (true) {

		uint64_t lota_version = 0;
		bnet_packet_t lota_packets[32] = {};
		int lota_packets_len = 0;

		if (uxQueueMessagesWaiting(state->lota_queue)) {
			xSemaphoreTake(state->lock, portMAX_DELAY);
			lota_version = state->lota_version;
			xSemaphoreGive(state->lock);

			do {
				xQueueReceive(state->lota_queue, lota_packets + lota_packets_len, portMAX_DELAY);
				ESP_LOGI(TAG, "TODO LOTA item...");
			} while (lota_packets[lota_packets_len][0] | lota_packets[lota_packets_len][1] && lota_packets_len < 32 && ++lota_packets_len);
			lota_packets_len += 1;
		}

		xSemaphoreTake(state->lock, portMAX_DELAY);
		bnet_packet_t sync = { state->ntp_time_usec + esp_timer_get_time() - state->esp_time_usec, lota_version };
		xSemaphoreGive(state->lock);

		xSemaphoreTake(state->lora, portMAX_DELAY);
		ESP_LOGW(TAG, "Broadcasting sync...");
		LoRa.idle();
		LoRa.beginPacket(true);
		LoRa.write((uint8_t *) bnet_header, sizeof bnet_header);
		LoRa.write((uint8_t *) sync, sizeof sync);
		LoRa.endPacket(true);
		LoRa.receive(sizeof (bnet_header_t) + sizeof (bnet_packet_t));
		xSemaphoreGive(state->lora);
		ESP_LOGI(TAG, "Broadcasted sync.");

		/*
		// TODO: send all ota packets

		xSemaphoreTake(state->lock, portMAX_DELAY);
		bool apply_ota_to_self = lota_version == state->version;
		xSemaphoreGive(state->lock);
		if (apply_ota_to_self) {
			// TODO: push all ota packets to ota queue
			for (int i = 0; i < lota_packets_len; i++)
				xQueueSendToBack(state->ota_queue, lota_packets + i, portMAX_DELAY);
		}
		*/

		delay(SYNC_SEC * MSECS_TO_SEC);
	}
}
