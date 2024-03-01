#include "main.hpp"
#include <esp_log.h>
#include <esp_timer.h>
#include <lwip/netdb.h>

#define TAG "ESP32resso TIME"

#define HOST "pool.ntp.org"
#define PORT (123)
#define TIMEOUT_SEC (10)

#define POLL_ERR_SEC (10)
#define POLL_MIN_SEC (5)
#define POLL_MAX_SEC (120)
#define MAX_DIFF_SEC (0.25)

#define USECS_TO_FRAC (0xFFFFFFFFull / USECS_TO_SEC)

struct ntp_packet {
	uint8_t li_vn_mode, stratum, poll, precision;
	uint32_t root_delay, root_dispersion, ref_id;
	uint32_t ref_time_secs, ref_time_frac;
	uint32_t orig_time_secs, orig_time_frac;
	uint32_t rx_time_secs, rx_time_frac;
	uint32_t tx_time_secs, tx_time_frac;
};

void bnet_time_gateway(void *raw_state)
{
	ESP_LOGI(TAG, "Starting gateway time task...");
	struct bnet_state *state = (struct bnet_state *) raw_state;

	while (true) {
		int delay_msec = POLL_ERR_SEC * MSECS_TO_SEC;

		int sock;
		struct timeval timeout = {};
		struct hostent *host;
		struct ntp_packet packet = {};
		struct sockaddr_in dst = {};
		struct sockaddr_storage src = {};
		socklen_t src_size = sizeof src;
		uint64_t esp_tx_usec, esp_rx_usec;
		uint64_t ntp_rx_secs, ntp_tx_secs;
		uint64_t new_ntp_time_usec, new_esp_time_usec;
		double time_diff_sec;

		ESP_LOGI(TAG, "Sending NTP request packet...");

		if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) < 0) {
			ESP_LOGE(TAG, "Unable to create new socket: %s", strerror(errno));
			goto end0;
		}
		timeout.tv_sec = TIMEOUT_SEC;
		if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout) < 0) {
			ESP_LOGE(TAG, "Unable to configure socket: %s", strerror(errno));
			goto end1;
		}

		if (!(host = gethostbyname(HOST)) || !host->h_addr) {
			ESP_LOGE(TAG, "Unable to get IP address of " HOST);
			goto end1;
		}
		dst.sin_family = AF_INET;
		dst.sin_addr = *((struct in_addr *) host->h_addr);
		dst.sin_port = htons(PORT);
		bzero(&dst.sin_zero, 8);

		packet.li_vn_mode = 0x1b;
		esp_tx_usec = esp_timer_get_time();
		if (sendto(sock, &packet, sizeof packet, 0, (struct sockaddr *) &dst, sizeof dst) < 0) {
			ESP_LOGE(TAG, "Unable to send request packet: %s", strerror(errno));
			goto end1;
		}
		if (recvfrom(sock, &packet, sizeof packet, 0, (struct sockaddr *) &src, &src_size) < 0) {
			ESP_LOGE(TAG, "Unable to receive response packet: %s", strerror(errno));
			goto end1;
		}
		esp_rx_usec = esp_timer_get_time();

		// Given the absolute time the server received, the absolute time the server responded and
		// the total time the exchange took, approximate the absolute time the client received
		ntp_rx_secs = ntohl(packet.rx_time_secs) * USECS_TO_SEC + ntohl(packet.rx_time_frac) / USECS_TO_FRAC;
		ntp_tx_secs = ntohl(packet.tx_time_secs) * USECS_TO_SEC + ntohl(packet.tx_time_frac) / USECS_TO_FRAC;
		new_ntp_time_usec = (ntp_rx_secs + ntp_tx_secs) / 2 + (esp_rx_usec - esp_tx_usec) / 2;
		new_esp_time_usec = esp_rx_usec;
		xSemaphoreTake(state->lock, portMAX_DELAY);
		time_diff_sec = ((int64_t) (new_ntp_time_usec - new_esp_time_usec) - (int64_t) (state->ntp_time_usec - state->esp_time_usec)) / (double) USECS_TO_SEC;
		state->ntp_time_usec = new_ntp_time_usec;
		state->esp_time_usec = new_esp_time_usec;
		xSemaphoreGive(state->lock);

		ESP_LOGW(TAG, "Received NTP response packet from %s - Time difference: %f secs", inet_ntoa(dst.sin_addr), time_diff_sec);

		// The next NTP request should be scheduled sooner if there was a large change in time
		delay_msec = MSECS_TO_SEC - constrain(abs(time_diff_sec / MAX_DIFF_SEC), 0, 1) * MSECS_TO_SEC;
		delay_msec = POLL_MIN_SEC * MSECS_TO_SEC + (POLL_MAX_SEC - POLL_MIN_SEC) * delay_msec;

end1:
		if (close(sock) < 0)
			ESP_LOGE(TAG, "Unable to close socket: %s", strerror(errno));
end0:
		ESP_LOGI(TAG, "Scheduled next NTP query: %f secs", delay_msec / (double) MSECS_TO_SEC);
		delay(delay_msec);
	}
}
