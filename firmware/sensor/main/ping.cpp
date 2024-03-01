#include "main.hpp"
#include <LoRa.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_http_client.h>

#define TAG "ESP32resso PING"

#define PING_MIN_MSEC (30 * MSECS_TO_SEC)
#define PING_MAX_MSEC (60 * MSECS_TO_SEC)

void bnet_ping(void *raw_state)
{
	ESP_LOGW(TAG, "Starting ping task...");
	struct bnet_state *state = (struct bnet_state *) raw_state;
	xSemaphoreTake(state->lock, portMAX_DELAY);
	bnet_packet_t ping = { ESP.getEfuseMac() & ~1, state->version }; // TODO: endianness
	xSemaphoreGive(state->lock);

	while (true) {
		ESP_LOGI(TAG, "Sending ping item...");
		xQueueSendToBack(state->ping_queue, ping, portMAX_DELAY);
		ESP_LOGI(TAG, "Sent ping item.");
		delay(random(PING_MIN_MSEC, PING_MAX_MSEC));
	}
}

void bnet_ping_gateway(void *raw_state)
{
	ESP_LOGW(TAG, "Starting gateway ping task...");
	struct bnet_state *state = (struct bnet_state *) raw_state;

		const esp_partition_t *new_partition = NULL;
		esp_ota_handle_t ota_handle;


	while (true) {
		ESP_LOGI(TAG, "Waiting for ping item...");
		bnet_packet_t ping;
		xQueueReceive(state->ping_queue, ping, portMAX_DELAY);
		ESP_LOGI(TAG, "Got ping item.");

		// TODO(optimisation): dedup queue contents

		uint64_t lota_version = ping[1];
		ESP_LOGW(TAG, "Checking for version %llu firmware updates...", lota_version);

		esp_err_t err;
		bnet_packet_t lota_terminator = {};
		esp_http_client_config_t http_config = {};
		esp_http_client_handle_t http_handle;
		int64_t content_length;
		int status_code;
		int read_len;

		http_config.host = OTA_HOST;
		http_config.port = OTA_PORT;
		http_config.path = OTA_PATH;
		if (!(http_handle = esp_http_client_init(&http_config))) {
			ESP_LOGE(TAG, "Unable to create HTTP client");
			goto end0;
		}

		if ((err = esp_http_client_open(http_handle, 0)) != ESP_OK) {
			ESP_LOGE(TAG, "Unable to open HTTP connection");
			goto end1;
		}

		if ((content_length = esp_http_client_fetch_headers(http_handle)) < 0) {
			ESP_LOGE(TAG, "Unable to read HTTP headers");
			goto end1;
		}

		if ((status_code = esp_http_client_get_status_code(http_handle)) != 200) {
			ESP_LOGE(TAG, "Received non-200 status code: %d", status_code);
			goto end1;
		}

		if (content_length == 0) {
			ESP_LOGI(TAG, "No firmware update found");
			goto end1;
		}

		ESP_LOGW(TAG, "Streaming v%lld firmware update diff...", lota_version);

		xSemaphoreTake(state->lock, portMAX_DELAY);
		state->lota_version = lota_version;
		xSemaphoreGive(state->lock);

		/*
		for (int i = 0; !esp_http_client_is_complete_data_received(http_handle); i++) {
			bnet_packet_t ota;

			ESP_LOGI(TAG, "Reading HTTP OTA item...");
			if ((read_len = esp_http_client_read(http_handle, (char *) ota, sizeof ota)) < 0) {
				ESP_LOGE(TAG, "Unable to read HTTP data");
				goto end1;
			}

			ESP_LOGI(TAG, "Pushing LOTA item...");
			// xQueueSendToBack(state->lota_queue, ota, portMAX_DELAY); // TODO: go via LOTA
			xQueueSendToBack(state->ota_queue, ota, portMAX_DELAY);
		}

		// xQueueSendToBack(state->lota_queue, lota_terminator, portMAX_DELAY);
		xQueueSendToBack(state->ota_queue, lota_terminator, portMAX_DELAY);
		ESP_LOGW(TAG, "Stream v%lld firmware update diff complete", lota_version);

end1:
		if ((err = esp_http_client_cleanup(http_handle)) != ESP_OK)
			ESP_LOGE(TAG, "HTTP cleanup error");
end0:
		while (uxQueueMessagesWaiting(state->lota_queue))
			delay(1 * MSECS_TO_SEC);

		xSemaphoreTake(state->lock, portMAX_DELAY);
		state->lota_version = 0;
		xSemaphoreGive(state->lock);
		*/

		ESP_LOGE(TAG, "Updating firmware...");

		if (!(new_partition = esp_ota_get_next_update_partition(NULL))) {
			ESP_LOGE(TAG, "Unable to find valid OTA partition to use");
			goto end1;
		}

		if ((err = esp_ota_begin(new_partition, OTA_SIZE_UNKNOWN, &ota_handle)) != ESP_OK) {
			ESP_LOGE(TAG, "Unable to start OTA");
			goto end1;
		}

		xSemaphoreTake(state->lock, portMAX_DELAY);
		state->ota_progress = 0;
		xSemaphoreGive(state->lock);

		for (int i = 0; !esp_http_client_is_complete_data_received(http_handle); i++) {
			// bnet_packet_t ota;
			char ota[1024];

			// ESP_LOGI(TAG, "Reading HTTP OTA item...");
			if ((read_len = esp_http_client_read(http_handle, (char *) ota, sizeof ota)) < 0) {
				ESP_LOGE(TAG, "Unable to read HTTP data");
				goto end2;
			}

			if ((err = esp_ota_write(ota_handle, ota, sizeof ota)) != ESP_OK) {
				ESP_LOGE(TAG, "OTA write error");
				goto end2;
			}

			xSemaphoreTake(state->lock, portMAX_DELAY);
			state->ota_progress += 1;
			xSemaphoreGive(state->lock);
		}

		// xQueueSendToBack(state->lota_queue, lota_terminator, portMAX_DELAY);
		// xQueueSendToBack(state->ota_queue, lota_terminator, portMAX_DELAY);
		ESP_LOGW(TAG, "Stream v%lld firmware update diff complete", lota_version);

		ESP_LOGE(TAG, "Finished firmware update.");

		if ((err = esp_ota_set_boot_partition(new_partition)) != ESP_OK) {
			ESP_LOGE(TAG, "OTA set boot partition error");
			goto end2;
		}

		ESP_LOGE(TAG, "Rebooting into new partition...");
		esp_restart();

end2:
		if ((err = esp_ota_abort(ota_handle)) != ESP_OK)
			ESP_LOGE(TAG, "OTA abort error");
end1:
		if ((err = esp_http_client_cleanup(http_handle)) != ESP_OK)
			ESP_LOGE(TAG, "HTTP cleanup error");
end0:
		while (uxQueueMessagesWaiting(state->lota_queue))
			delay(1 * MSECS_TO_SEC);

		xSemaphoreTake(state->lock, portMAX_DELAY);
		state->lota_version = 0;
		xSemaphoreGive(state->lock);
	}
}

void bnet_ping_remote(void *raw_state)
{
	ESP_LOGW(TAG, "Starting remote ping task...");
	struct bnet_state *state = (struct bnet_state *) raw_state;

	while (true) {
		ESP_LOGI(TAG, "Waiting for ping item...");
		bnet_packet_t ping;
		xQueueReceive(state->ping_queue, ping, portMAX_DELAY);
		ESP_LOGI(TAG, "Got ping item.");

		xSemaphoreTake(state->lora, portMAX_DELAY);
		ESP_LOGW(TAG, "Broadcasting ping...");
		LoRa.idle();
		LoRa.beginPacket(true);
		LoRa.write((uint8_t *) bnet_header, sizeof bnet_header);
		LoRa.write((uint8_t *) ping, sizeof ping);
		LoRa.endPacket(true);
		LoRa.receive(sizeof (bnet_header_t) + sizeof (bnet_packet_t));
		xSemaphoreGive(state->lora);
		ESP_LOGI(TAG, "Broadcasted ping.");
	}
}
