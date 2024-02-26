#include "ota.hpp"
#include "ntp.hpp"
#include <Arduino.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_http_client.h>
#include <freertos/timers.h>

#define TAG "ESP32resso OTA"

#define TASK_STACK_SIZE (8096)
#define TASK_PRIORITY (5)

// NOTE: Run `python3 -m http.server` in /firmware/sensor directory
#define HOST "192.168.0.179"
#define PORT (8000)
#define PATH "/build/sensor.bin"

#define MIN_DELAY_SECS (3600)
#define MAX_DELAY_SECS (7200)
#define ERR_DELAY_SECS (1800)

// Query for firmware updates, update firmware if newer, else schedule the next query
static void task(void *args)
{
	TimerHandle_t timer = (TimerHandle_t) args;
	int delay_msec = ERR_DELAY_SECS * MSECS_TO_SEC;

	esp_err_t err;
	const esp_app_desc_t *app_desc;
	esp_http_client_config_t http_config = {};
	esp_http_client_handle_t http_handle;
	const esp_partition_t *new_partition;
	esp_ota_handle_t ota_handle;
	int64_t content_length;
	int status_code;
	char read_buf[1024];
	int read_len;

	ESP_LOGI(TAG, "Checking for firmware updates...");

	app_desc = esp_app_get_description();

	http_config.host = HOST;
	http_config.port = PORT;
	http_config.path = PATH;
	if (!(http_handle = esp_http_client_init(&http_config))) {
		ESP_LOGE(TAG, "Unable to create HTTP client");
		goto task_abort0;
	}

	if ((err = esp_http_client_open(http_handle, 0)) != ESP_OK) {
		ESP_LOGE(TAG, "Unable to open HTTP connection");
		goto task_abort1;
	}

	if ((content_length = esp_http_client_fetch_headers(http_handle)) < 0) {
		ESP_LOGE(TAG, "Unable to read HTTP headers");
		goto task_abort1;
	}

	if ((status_code = esp_http_client_get_status_code(http_handle)) != 200) {
		ESP_LOGE(TAG, "Received non-200 status code: %d", status_code);
		goto task_abort1;
	}

	if (true) { // TODO: check if version is newer than current
		ESP_LOGI(TAG, "No new firmware update found");
		delay_msec = random(MIN_DELAY_SECS * MSECS_TO_SEC, MAX_DELAY_SECS * MSECS_TO_SEC);
		goto task_abort1;
	}

	ESP_LOGI(TAG, "Updating firmware...");

	if (!(new_partition = esp_ota_get_next_update_partition(NULL))) {
		ESP_LOGE(TAG, "Unable to find valid OTA partition to use");
		goto task_abort1;
	}

	if ((err = esp_ota_begin(new_partition, OTA_SIZE_UNKNOWN, &ota_handle)) != ESP_OK) {
		ESP_LOGE(TAG, "Unable to start OTA");
		goto task_abort1;
	}

	// Write HTTP response chunk by chunk into new boot partition
	// TODO: delta updates?
	while (!esp_http_client_is_complete_data_received(http_handle)) {
		if ((read_len = esp_http_client_read(http_handle, read_buf, sizeof read_buf)) < 0) {
			ESP_LOGE(TAG, "Unable to read HTTP data");
			goto task_abort2;
		} else if ((err = esp_ota_write(ota_handle, read_buf, read_len)) != ESP_OK) {
			ESP_LOGE(TAG, "OTA write error");
			goto task_abort2;
		}
	}

	if ((err = esp_ota_end(ota_handle)) != ESP_OK) {
		ESP_LOGE(TAG, "OTA end error");
		goto task_abort2;
	}

	if ((err = esp_ota_set_boot_partition(new_partition)) != ESP_OK) {
		ESP_LOGE(TAG, "OTA set boot partition error");
		goto task_abort2;
	}

	ESP_LOGI(TAG, "Rebooting into new partition...");
	esp_restart();

task_abort2:
	if ((err = esp_ota_abort(ota_handle)) != ESP_OK)
		ESP_LOGE(TAG, "OTA abort error");
task_abort1:
	if ((err = esp_http_client_cleanup(http_handle)) != ESP_OK)
		ESP_LOGE(TAG, "HTTP cleanup error");
task_abort0:
	ESP_LOGI(TAG, "Scheduled next query: %f secs", delay_msec / (double) MSECS_TO_SEC);
	xTimerChangePeriod(timer, delay_msec / portTICK_PERIOD_MS, portMAX_DELAY);
	xTimerReset(timer, portMAX_DELAY);
	vTaskDelete(NULL);
}

// Perform the next OTA query
static void on_timeout(TimerHandle_t timer)
{
	if (xTaskCreate(task, "ota task", TASK_STACK_SIZE, timer, TASK_PRIORITY, NULL) != pdPASS) {
		ESP_LOGE(TAG, "Unable to create task, trying again in %d seconds...", ERR_DELAY_SECS);
		xTimerChangePeriod(timer, (ERR_DELAY_SECS * MSECS_TO_SEC) / portTICK_PERIOD_MS, portMAX_DELAY);
		xTimerReset(timer, portMAX_DELAY);
	}
}

// Regularly query server for new firmware updates
void bnet_ota_start()
{
	TimerHandle_t timer = xTimerCreate("ota_timer", 1, false, NULL, on_timeout);
	if (!timer || xTimerStart(timer, portMAX_DELAY) != pdPASS)
		ESP_LOGE(TAG, "Timer init error");
}
