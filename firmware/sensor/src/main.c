#include <string.h>
#include <nvs_flash.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_timer.h>
#include <lwip/netdb.h>

#define TAG "Team ESP32resso"

#define WIFI_IF_DESC "wifi_interface"
#define WIFI_SSID "msungie"
#define WIFI_PSWD "hamandcheese"

#define NTP_HOST "pool.ntp.org"
#define NTP_PORT 123

#define USECS_IN_SEC (1000000ll)
#define NTP_UNIX_DIFFERENCE (2208988800ll)
#define USECS_IN_UINT32 (0xFFFFFFFFull / USECS_IN_SEC)

static int64_t esp_time_us = 0;
static int64_t ntp_time_us = 0;

typedef struct ntp_packet {
	uint8_t li_vn_mode;
	uint8_t stratum;
	uint8_t poll;
	uint8_t precision;
	uint32_t root_delay;
	uint32_t root_dispersion;
	uint32_t ref_id;
	uint32_t ref_time_secs;
	uint32_t ref_time_frac;
	uint32_t orig_time_secs;
	uint32_t orig_time_frac;
	uint32_t rx_time_secs;
	uint32_t rx_time_frac;
	uint32_t tx_time_secs;
	uint32_t tx_time_frac;
} ntp_packet_t;

static void on_wifi_connect(void *esp_netif, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	ESP_LOGI(TAG, "Wi-Fi connected.");
}

static void on_wifi_disconnect(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	ESP_LOGI(TAG, "Wi-Fi disconnected.");
}

static void on_sta_got_ip(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	SemaphoreHandle_t *semph_get_ip_addr = arg;
	ip_event_got_ip_t *event = event_data;
	ESP_LOGI(TAG, "New IPv4 address: " IPSTR, IP2STR(&event->ip_info.ip));
	xSemaphoreGive(semph_get_ip_addr);
}

static esp_netif_t *start_wifi(void)
{
	ESP_LOGI(TAG, "Starting Wi-Fi...");
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	esp_netif_inherent_config_t netif_config = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
	netif_config.if_desc = WIFI_IF_DESC;
	netif_config.route_prio = 128;
	esp_netif_t *sta_netif = esp_netif_create_wifi(WIFI_IF_STA, &netif_config);
	ESP_ERROR_CHECK(esp_wifi_set_default_wifi_sta_handlers());
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_start());
	return sta_netif;
}

static void connect_wifi(void)
{
	ESP_LOGI(TAG, "Connecting to WI-FI AP " WIFI_SSID "...");
	SemaphoreHandle_t semph_get_ip_addr = semph_get_ip_addr = xSemaphoreCreateBinary();
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &on_wifi_connect, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &on_wifi_disconnect, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_sta_got_ip, semph_get_ip_addr));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &(wifi_config_t) {
		.sta = {
			.ssid = WIFI_SSID,
			.password = WIFI_PSWD,
			.scan_method = WIFI_FAST_SCAN,
			.sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
			.threshold.rssi = -127,
			.threshold.authmode = WIFI_AUTH_OPEN,
		}
	}));
	ESP_ERROR_CHECK(esp_wifi_connect());
	xSemaphoreTake(semph_get_ip_addr, portMAX_DELAY);
}

static void ntp_client_task(void *arg)
{
	while (1) {
		ESP_LOGI(TAG, "Creating new socket...");
		int sock;
		if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) < 0) {
			ESP_LOGE(TAG, "Unable to create socket: %s", strerror(errno));
			break;
		}

		ESP_LOGI(TAG, "Setting socket timeout to 10 seconds...");
		struct timeval timeout = { .tv_sec = 10, .tv_usec = 0 };
		if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout) < 0) {
			ESP_LOGE(TAG, "Unable to set socket timeout option: errno %d", errno);
			break;
		}

		while (1) {
			ESP_LOGI(TAG, "Fetching IP address of " NTP_HOST "...");
			struct sockaddr_in dst;
			dst.sin_family = AF_INET;
			dst.sin_addr = *((struct in_addr *) gethostbyname(NTP_HOST)->h_addr);
			dst.sin_port = htons(NTP_PORT);
			bzero(&dst.sin_zero, 8);
			socklen_t dst_size = sizeof dst;

			ESP_LOGI(TAG, "Sending NTP packet to %s:%d...", inet_ntoa(dst.sin_addr), NTP_PORT);
			ntp_packet_t packet = {0};
			packet.li_vn_mode = 0x1b;
			if (sendto(sock, &packet, sizeof packet, 0, (struct sockaddr *) &dst, dst_size) < 0) {
				ESP_LOGE(TAG, "Unable to send packet: %s", strerror(errno));
				break;
			}

			ESP_LOGI(TAG, "Waiting for NTP packet response...");
			struct sockaddr_storage src;
			socklen_t src_size = sizeof src;
			if (recvfrom(sock, &packet, sizeof packet, 0, (struct sockaddr *) &src, &src_size) < 0) {
				ESP_LOGE(TAG, "Unable to receive packet: %s", strerror(errno));
				break;
			}

			int64_t new_esp_time_us = esp_timer_get_time();
			int64_t secs = (int64_t) ntohl(packet.tx_time_secs) - NTP_UNIX_DIFFERENCE;
			int64_t frac = (int64_t) ntohl(packet.tx_time_frac) / USECS_IN_UINT32;
			int64_t new_ntp_time_us = secs * USECS_IN_SEC + frac;
			int64_t difference = (new_ntp_time_us - ntp_time_us) - (new_esp_time_us - esp_time_us);
			if (esp_time_us == 0) {
				ESP_LOGI(TAG, "NTP time received.");
				ntp_time_us = new_ntp_time_us;
				esp_time_us = new_esp_time_us;
			} else {
				ESP_LOGI(TAG, "NTP time update received. Difference: %f secs", ((double) difference) / USECS_IN_SEC);
				ntp_time_us = new_ntp_time_us - difference / 2;
				esp_time_us = new_esp_time_us - difference / 2;
			}

			vTaskDelay(10123 / portTICK_PERIOD_MS);
		}

		if (sock != -1) {
			shutdown(sock, 0);
			close(sock);
		}

		vTaskDelay(5000 / portTICK_PERIOD_MS);
	}

	vTaskDelete(NULL);
}


static void print_task(void *arg)
{
	TickType_t latest = xTaskGetTickCount();
	while (1) {
		vTaskDelayUntil(&latest, 100 / portTICK_PERIOD_MS);
		int64_t time_us = ntp_time_us + esp_timer_get_time() - esp_time_us;
		ESP_LOGI(TAG, "%f secs", ((double) time_us) / USECS_IN_SEC);
	}
}

void app_main(void)
{
	ESP_ERROR_CHECK(nvs_flash_init());
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	start_wifi();
	connect_wifi();
	xTaskCreate(ntp_client_task, "ntp_client", 8096, NULL, 5, NULL);
	xTaskCreate(print_task, "print", 8096, NULL, 5, NULL);
}
