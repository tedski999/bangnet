#include "main.hpp"
#include <esp_log.h>
#include <esp_ota_ops.h>
// #include <esp_http_client.h>

#define TAG "ESP32resso OTA"

void bnet_ota(void *raw_state)
{
	ESP_LOGW(TAG, "Starting OTA task...");
	struct bnet_state *state = (struct bnet_state *) raw_state;
	const esp_partition_t *new_partition = NULL;
	esp_ota_handle_t ota_handle;

	while (true) {
		ESP_LOGI(TAG, "Waiting for OTA item...");
		bnet_packet_t ota;
		xQueueReceive(state->ota_queue, ota, portMAX_DELAY);
		ESP_LOGI(TAG, "Got OTA item.");

		esp_err_t err;

		if (!new_partition) {
			ESP_LOGE(TAG, "Updating firmware...");

			if (!(new_partition = esp_ota_get_next_update_partition(NULL))) {
				ESP_LOGE(TAG, "Unable to find valid OTA partition to use");
				goto end0;
			}

			if ((err = esp_ota_begin(new_partition, OTA_SIZE_UNKNOWN, &ota_handle)) != ESP_OK) {
				ESP_LOGE(TAG, "Unable to start OTA");
				goto end0;
			}

			xSemaphoreTake(state->lock, portMAX_DELAY);
			state->ota_progress = 0;
			xSemaphoreGive(state->lock);
		}

		if (ota[0] | ota[1]) {
			ESP_LOGW(TAG, "Applying delta OTA chunk...");

			if ((err = esp_ota_write(ota_handle, ota, sizeof ota)) != ESP_OK) {
				ESP_LOGE(TAG, "OTA write error");
				goto end1;
			}

			xSemaphoreTake(state->lock, portMAX_DELAY);
			state->ota_progress += 1;
			xSemaphoreGive(state->lock);

		} else {
			ESP_LOGE(TAG, "Finished firmware update.");

			if ((err = esp_ota_set_boot_partition(new_partition)) != ESP_OK) {
				ESP_LOGE(TAG, "OTA set boot partition error");
				goto end1;
			}

			ESP_LOGE(TAG, "Rebooting into new partition...");
			esp_restart();
		}

		continue;

end1:
		if ((err = esp_ota_abort(ota_handle)) != ESP_OK)
			ESP_LOGE(TAG, "OTA abort error");
end0:
		new_partition = NULL;
		xSemaphoreTake(state->lock, portMAX_DELAY);
		state->ota_progress = 0;
		xSemaphoreGive(state->lock);
	}
}


/*

		if (!(new_partition = esp_ota_get_next_update_partition(NULL))) {
			ESP_LOGE(TAG, "Unable to find valid OTA partition to use");
			goto gateway_ota_task_abort1;
		}

		if ((err = esp_ota_begin(new_partition, OTA_SIZE_UNKNOWN, &ota_handle)) != ESP_OK) {
			ESP_LOGE(TAG, "Unable to start OTA");
			goto gateway_ota_task_abort1;
		}

		// Write HTTP response chunk by chunk into new boot partition
		while (!esp_http_client_is_complete_data_received(http_handle)) {
			if ((read_len = esp_http_client_read(http_handle, read_buf, sizeof read_buf)) < 0) {
				ESP_LOGE(TAG, "Unable to read HTTP data");
				goto gateway_ota_task_abort2;
			} else if ((err = esp_ota_write(ota_handle, read_buf, read_len)) != ESP_OK) {
				ESP_LOGE(TAG, "OTA write error");
				goto gateway_ota_task_abort2;
			}
		}

		if ((err = esp_ota_end(ota_handle)) != ESP_OK) {
			ESP_LOGE(TAG, "OTA end error");
			goto gateway_ota_task_abort2;
		}

		if ((err = esp_ota_set_boot_partition(new_partition)) != ESP_OK) {
			ESP_LOGE(TAG, "OTA set boot partition error");
			goto gateway_ota_task_abort2;
		}

		ESP_LOGI(TAG, "Rebooting into new partition...");
		esp_restart();
*/
