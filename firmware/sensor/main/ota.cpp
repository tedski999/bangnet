#include "ota.hpp"
#include "ntp.hpp"
#include <Arduino.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_http_client.h>
#include <freertos/timers.h>

#define TAG "ESP32resso OTA"

#define HOST "192.168.0.179"
#define PORT (80)
#define MIN_POLL_INTERVAL_SECS (6)
#define MAX_POLL_INTERVAL_SECS (12)

static void on_timeout(TimerHandle_t timer)
{
	ESP_LOGI(TAG, "Checking for firmware updates...");

	/*

	const esp_app_desc_t *app_desc = esp_app_get_description();

	esp_http_client_config_t http_config;
	http_config.url = HOST;

	esp_http_client_handle_t http_handle = esp_http_client_init(&http_config);
	if (!http_handle) {
		ESP_LOGE(TAG, "Unable to create HTTP client");
		goto on_timeout_end0;
	}

	esp_err_t err;
	esp_ota_handle_t ota_handle;

	{
		ESP_LOGI(TAG, "Querying for firmware...");

		err = esp_http_client_open(http_handle, 0);
		if (err != ESP_OK) {
			ESP_LOGE(TAG, "Unable to open HTTP connection");
			goto on_timeout_end1;
		}

		int64_t content_length = esp_http_client_fetch_headers(http_handle);
		if (content_length < 0) {
			ESP_LOGE(TAG, "Unable to read HTTP headers");
			goto on_timeout_end2;
		}

		int status_code = esp_http_client_get_status_code(http_handle);
		if (status_code != 200) {
			ESP_LOGE(TAG, "Received non-200 status code: %d", status_code);
			goto on_timeout_end2;
		}

		ESP_LOGI(TAG, "Updating firmware...");
		// TODO: query for new updates
		// ESP_LOGI(TAG, "Updating to firmware version %s", new_app_ver);
		// if (new_app_ver[0] <= app_ver[0]) {
		// 	ESP_LOGI(TAG, "No new firmware update found");
		// 	goto on_timeout_end0;
		// }

		const esp_partition_t *new_partition = esp_ota_get_next_update_partition(NULL);
		if (new_partition) {
			ESP_LOGI(TAG, "Unable to find valid OTA partition to use");
			goto on_timeout_end2;
		}

		err = esp_ota_begin(new_partition, OTA_SIZE_UNKNOWN, &ota_handle);
		if (err != ESP_OK) {
			ESP_LOGI(TAG, "Unable to start OTA");
			goto on_timeout_end2;
		}

		while (!esp_http_client_is_complete_data_received(http_handle)) {
			char buf[1024];
			int len = sizeof buf;
			len = esp_http_client_read(http_handle, buf, len);
			if (len < 0) {
				ESP_LOGE(TAG, "Unable to read HTTP data");
				goto on_timeout_end3;
			}
			err = esp_ota_write(ota_handle, buf, len);
			if (err != ESP_OK) {
				ESP_LOGE(TAG, "OTA write error");
				goto on_timeout_end3;
			}
		}

		err = esp_ota_end(ota_handle);
		if (err != ESP_OK) {
			ESP_LOGE(TAG, "OTA end error");
		}

		err = esp_ota_set_boot_partition(new_partition);
		if (err != ESP_OK) {
			ESP_LOGE(TAG, "OTA set boot partition error");
		}

		esp_restart();
	}

on_timeout_end3:
	err = esp_ota_abort(ota_handle);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "OTA abort error");
	}

on_timeout_end2:
	esp_http_client_close(http_handle);

on_timeout_end1:
	esp_http_client_cleanup(http_handle);

on_timeout_end0:
	long delay = random(MIN_POLL_INTERVAL_SECS * MSECS_TO_SEC, MAX_POLL_INTERVAL_SECS * MSECS_TO_SEC);
	xTimerChangePeriod(timer, delay / portTICK_PERIOD_MS, 0);
	xTimerReset(timer, 0);
	ESP_LOGI(TAG, "Scheduled next query: %f secs", ((double) delay) / MSECS_TO_SEC);
	*/
}

void bnet_ota_start() {
	TimerHandle_t timer = xTimerCreate("ota_timer", 1, true, NULL, on_timeout);
	xTimerStart(timer, 0);
}
