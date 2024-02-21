#include "ntp.hpp"
#include <Arduino.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <lwip/netdb.h>
#include <freertos/timers.h>

#define TAG "ESP32resso NTP"

#define TASK_STACK_SIZE (8096)
#define TASK_PRIORITY (5)

// Perform NTP against public pool, giving up after 10 seconds
#define HOST "pool.ntp.org"
#define PORT (123)
#define TIMEOUT_SECS (10)

// Perform NTP requests every 5 to 120 seconds, or after 10 seconds if an error occurred
// The delay depends on how large of a time difference the last NTP response caused
#define MIN_DELAY_SECS (5)
#define MAX_DELAY_SECS (120)
#define MAX_DIFFERENCE_SECS (0.25)
#define ERR_DELAY_SECS (10)

// Convert a fraction of a second stored in a uint32_t to microseconds
#define USECS_TO_FRAC (0xFFFFFFFFull / USECS_TO_SEC)

struct packet {
	uint8_t li_vn_mode, stratum, poll, precision;
	uint32_t root_delay, root_dispersion, ref_id;
	uint32_t ref_time_secs, ref_time_frac;
	uint32_t orig_time_secs, orig_time_frac;
	uint32_t rx_time_secs, rx_time_frac;
	uint32_t tx_time_secs, tx_time_frac;
};

static SemaphoreHandle_t mutex;
static int64_t ntp_time_usec = 0;
static int64_t esp_time_usec = 0;

// Perform a NTP request then schedule the next one
static void task(void *args)
{
	TimerHandle_t timer = (TimerHandle_t) args;
	int delay_msec = ERR_DELAY_SECS * MSECS_TO_SEC;

	int sock;
	struct timeval timeout = {};
	struct hostent *host;
	struct packet packet = {};
	struct sockaddr_in dst = {};
	struct sockaddr_storage src = {};
	socklen_t src_size = sizeof src;
	int64_t esp_tx_usec, esp_rx_usec;
	int64_t ntp_rx_secs, ntp_tx_secs;
	int64_t new_ntp_time_usec, new_esp_time_usec;
	double difference_sec;

	ESP_LOGI(TAG, "Fetching current time...");

	// Setup a UDP socket to perform NTP request
	if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) < 0) {
		ESP_LOGE(TAG, "Unable to create new socket: %s", strerror(errno));
		goto task_abort0;
	}
	timeout.tv_sec = TIMEOUT_SECS;
	if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout) < 0) {
		ESP_LOGE(TAG, "Unable to configure socket: %s", strerror(errno));
		goto task_abort1;
	}

	// Fetch IP address of the NTP server to be queried
	if (!(host = gethostbyname(HOST)) || !host->h_addr) {
		ESP_LOGE(TAG, "Unable to get IP address of " HOST);
		goto task_abort1;
	}
	dst.sin_family = AF_INET;
	dst.sin_addr = *((struct in_addr *) host->h_addr);
	dst.sin_port = htons(PORT);
	bzero(&dst.sin_zero, 8);

	// Send and receive UDP datagrams containing the NTP packets
	packet.li_vn_mode = 0x1b;
	esp_tx_usec = esp_timer_get_time();
	if (sendto(sock, &packet, sizeof packet, 0, (struct sockaddr *) &dst, sizeof dst) < 0) {
		ESP_LOGE(TAG, "Unable to send packet: %s", strerror(errno));
		goto task_abort1;
	}

	if (recvfrom(sock, &packet, sizeof packet, 0, (struct sockaddr *) &src, &src_size) < 0) {
		ESP_LOGE(TAG, "Unable to receive packet: %s", strerror(errno));
		goto task_abort1;
	}

	// Given the absolute time the server received, the absolute time the server responded and
	// the total time the exchange took, approximate the absolute time the client received
	esp_rx_usec = esp_timer_get_time();
	ntp_rx_secs = ntohl(packet.rx_time_secs) * USECS_TO_SEC + ntohl(packet.rx_time_frac) / USECS_TO_FRAC;
	ntp_tx_secs = ntohl(packet.tx_time_secs) * USECS_TO_SEC + ntohl(packet.tx_time_frac) / USECS_TO_FRAC;
	new_ntp_time_usec = (ntp_rx_secs + ntp_tx_secs) / 2 + (esp_rx_usec - esp_tx_usec) / 2;
	new_esp_time_usec = esp_rx_usec;
	difference_sec = ((new_ntp_time_usec - new_esp_time_usec) - (ntp_time_usec - esp_time_usec)) / (double) USECS_TO_SEC;

	xSemaphoreTake(mutex, portMAX_DELAY);
	ntp_time_usec = new_ntp_time_usec;
	esp_time_usec = new_esp_time_usec;
	xSemaphoreGive(mutex);

	ESP_LOGI(TAG, "Received new time from %s", inet_ntoa(dst.sin_addr));
	ESP_LOGI(TAG, "Time difference: %f secs", difference_sec);

	// The next NTP request should be scheduled sooner if there was a large change in time
	delay_msec = MSECS_TO_SEC - constrain(abs(difference_sec / MAX_DIFFERENCE_SECS), 0, 1) * MSECS_TO_SEC;
	delay_msec = MIN_DELAY_SECS * MSECS_TO_SEC + (MAX_DELAY_SECS - MIN_DELAY_SECS) * delay_msec;

task_abort1:
	if (close(sock) < 0)
		ESP_LOGE(TAG, "Unable to close socket: %s", strerror(errno));
task_abort0:
	ESP_LOGI(TAG, "Scheduled next query: %f secs", delay_msec / (double) MSECS_TO_SEC);
	xTimerChangePeriod(timer, delay_msec / portTICK_PERIOD_MS, portMAX_DELAY);
	xTimerReset(timer, portMAX_DELAY);
	vTaskDelete(NULL);
}

// Perform the next NTP request
static void on_timeout(TimerHandle_t timer)
{
	if (xTaskCreate(task, "ntp task", TASK_STACK_SIZE, timer, TASK_PRIORITY, NULL) != pdPASS) {
		ESP_LOGE(TAG, "Unable to create task, trying again in %d seconds...", ERR_DELAY_SECS);
		xTimerChangePeriod(timer, (ERR_DELAY_SECS * MSECS_TO_SEC) / portTICK_PERIOD_MS, portMAX_DELAY);
		xTimerReset(timer, portMAX_DELAY);
	}
}

// Regularly perform NTP requests to keep clocks synchronised
void bnet_ntp_start()
{
	TimerHandle_t timer = xTimerCreate("ntp_timer", 1, false, NULL, on_timeout);
	if (!timer || xTimerStart(timer, portMAX_DELAY) != pdPASS)
		ESP_LOGE(TAG, "Timer init error");
	if (!(mutex = xSemaphoreCreateMutex()))
		ESP_LOGE(TAG, "Mutex init error");
}

// Get time since epoch according to latest NTP response, otherwise return 0
int64_t bnet_ntp_time_usec()
{
	int64_t time_usec = 0;
	if (mutex) {
		xSemaphoreTake(mutex, portMAX_DELAY);
		if (ntp_time_usec)
			time_usec = ntp_time_usec + esp_timer_get_time() - esp_time_usec;
		xSemaphoreGive(mutex);
	}
	return time_usec;
}
