#include "ntp.hpp"
#include <Arduino.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <lwip/netdb.h>
#include <freertos/timers.h>

#define TAG "ESP32resso NTP"

#define HOST "pool.ntp.org"
#define PORT (123)

#define MIN_POLL_INTERVAL_SECS (60)
#define MAX_POLL_INTERVAL_SECS (120)
#define MAX_RESPONSE_DELAY_SECS (10)

#define USECS_TO_FRAC (0xFFFFFFFFull / USECS_TO_SEC)

struct packet {
	uint8_t li_vn_mode, stratum, poll, precision;
	uint32_t root_delay, root_dispersion, ref_id;
	uint32_t ref_time_secs, ref_time_frac;
	uint32_t orig_time_secs, orig_time_frac;
	uint32_t rx_time_secs, rx_time_frac;
	uint32_t tx_time_secs, tx_time_frac;
};

static int64_t ntp_time_usec = 0;
static int64_t esp_time_usec = 0;

static void on_timeout(TimerHandle_t timer)
{
	ESP_LOGI(TAG, "Creating new socket...");
	int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (sock < 0) {
		ESP_LOGE(TAG, "Unable to create new socket: %s", strerror(errno));
		goto on_ntp_poll_end0;
	}

	{
		ESP_LOGI(TAG, "Configuring socket...");
		struct timeval timeout = { .tv_sec = MAX_RESPONSE_DELAY_SECS, .tv_usec = 0 };
		if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout) < 0) {
			ESP_LOGE(TAG, "Unable to configure socket: %s", strerror(errno));
			goto on_ntp_poll_end1;
		}

		ESP_LOGI(TAG, "Fetching IP address of " HOST "...");
		struct sockaddr_in dst;
		struct hostent *host = gethostbyname(HOST);
		if (!host || !host->h_addr) {
			ESP_LOGE(TAG, "Unable to get IP address of " HOST);
			goto on_ntp_poll_end1;
		}
		dst.sin_family = AF_INET;
		dst.sin_addr = *((struct in_addr *) host->h_addr);
		dst.sin_port = htons(PORT);
		bzero(&dst.sin_zero, 8);

		ESP_LOGI(TAG, "Sending packet to %s:%d...", inet_ntoa(dst.sin_addr), PORT);
		struct packet packet;
		packet.li_vn_mode = 0x1b;
		int64_t esp_tx_usec = esp_timer_get_time();
		if (sendto(sock, &packet, sizeof packet, 0, (struct sockaddr *) &dst, sizeof dst) < 0) {
			ESP_LOGE(TAG, "Unable to send packet: %s", strerror(errno));
			goto on_ntp_poll_end1;
		}

		struct sockaddr_storage src;
		socklen_t src_size = sizeof src;
		if (recvfrom(sock, &packet, sizeof packet, 0, (struct sockaddr *) &src, &src_size) < 0) {
			ESP_LOGE(TAG, "Unable to receive packet: %s", strerror(errno));
			goto on_ntp_poll_end1;
		}

		ESP_LOGI(TAG, "Processing received packet...");
		int64_t esp_rx_usec = esp_timer_get_time();
		int64_t ntp_rx_secs = ntohl(packet.rx_time_secs) * USECS_TO_SEC + ntohl(packet.rx_time_frac) / USECS_TO_FRAC;
		int64_t ntp_tx_secs = ntohl(packet.tx_time_secs) * USECS_TO_SEC + ntohl(packet.tx_time_frac) / USECS_TO_FRAC;
		int64_t esp_dt_usec = esp_rx_usec - esp_tx_usec;
		int64_t new_ntp_time_usec = (ntp_rx_secs + ntp_tx_secs) / 2 + esp_dt_usec / 2;
		int64_t new_esp_time_usec = esp_rx_usec;
		int64_t difference_usec = (new_ntp_time_usec - new_esp_time_usec) - (ntp_time_usec - esp_time_usec);

		ESP_LOGI(TAG, "Time difference: %f secs", ((double) difference_usec) / USECS_TO_SEC);
		ntp_time_usec = new_ntp_time_usec;
		esp_time_usec = new_esp_time_usec;
	}

on_ntp_poll_end1:
	shutdown(sock, 0);
	close(sock);

on_ntp_poll_end0:
	long delay = random(MIN_POLL_INTERVAL_SECS * MSECS_TO_SEC, MAX_POLL_INTERVAL_SECS * MSECS_TO_SEC);
	xTimerChangePeriod(timer, delay / portTICK_PERIOD_MS, 0);
	xTimerReset(timer, 0);
	ESP_LOGI(TAG, "Scheduled next query: %f secs", ((double) delay) / MSECS_TO_SEC);
}

void bnet_ntp_start()
{
	TimerHandle_t timer = xTimerCreate("ntp_timer", 1, true, NULL, on_timeout);
	xTimerStart(timer, 0);
}

int64_t bnet_ntp_time_usec()
{
	return ntp_time_usec + esp_timer_get_time() - esp_time_usec;
}
