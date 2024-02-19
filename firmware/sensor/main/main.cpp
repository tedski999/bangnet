#include <sdkconfig.h>
#include <Arduino.h>
#include <WiFi.h>
#include <esp_log.h>
#include <lwip/netdb.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

#define TAG "ESP32resso"

#define SDA 4
#define SCL 15

#define OLED_RST 16
#define OLED_ADDR 0x3c
#define OLED_WIDTH 128
#define OLED_HEIGHT 64

#define WIFI_SSID "msungie"
#define WIFI_PSWD "hamandcheese"

#define OTA_HOST "192.168.0.173"
#define OTA_PORT 80
#define OTA_MIN_POLL_INTERVAL_SECS 600
#define OTA_MAX_POLL_INTERVAL_SECS 1200

#define NTP_HOST "pool.ntp.org"
#define NTP_PORT 123
#define NTP_MIN_POLL_INTERVAL_SECS 60
#define NTP_MAX_POLL_INTERVAL_SECS 120
#define NTP_MAX_RESPONSE_DELAY_SECS 10

#define MSECS_TO_SEC (1000ll)
#define USECS_TO_SEC (MSECS_TO_SEC * 1000ll)
#define USECS_TO_FRAC (0xFFFFFFFFull / USECS_TO_SEC)

static int64_t ntp_time_usec = 0;
static int64_t esp_time_usec = 0;

static Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RST);

static void ota_task(void *arg)
{
	while (true) {
		// TODO: ota http or https?

		while (true) {
			// TODO: ota delta updates?

			long poll_delay = random(OTA_MIN_POLL_INTERVAL_SECS * MSECS_TO_SEC, OTA_MAX_POLL_INTERVAL_SECS * MSECS_TO_SEC);
			ESP_LOGI(TAG " OTA", "Scheduled next poll in %f secs", ((double) poll_delay) / MSECS_TO_SEC);
			delay(poll_delay);
		}

		delay(5 * MSECS_TO_SEC);
	}

	vTaskDelete(NULL);
}

static void ntp_task(void *arg)
{
	while (true) {
		ESP_LOGI(TAG " NTP", "Creating new socket...");
		int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
		struct timeval timeout = { .tv_sec = NTP_MAX_RESPONSE_DELAY_SECS, .tv_usec = 0 };
		if (sock < 0 || setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout) < 0) {
			ESP_LOGE(TAG " NTP", "Unable to configure new socket: %s", strerror(errno));
			break;
		}

		while (true) {
			ESP_LOGI(TAG " NTP", "Fetching IP address of " NTP_HOST "...");
			struct sockaddr_storage src;
			socklen_t src_size = sizeof src;
			struct sockaddr_in dst;
			dst.sin_family = AF_INET;
			dst.sin_addr = *((struct in_addr *) gethostbyname(NTP_HOST)->h_addr);
			dst.sin_port = htons(NTP_PORT);
			bzero(&dst.sin_zero, 8);
			socklen_t dst_size = sizeof dst;


			ESP_LOGI(TAG " NTP", "Sending packet to %s:%d...", inet_ntoa(dst.sin_addr), NTP_PORT);
			struct {
				uint8_t li_vn_mode, stratum, poll, precision;
				uint32_t root_delay, root_dispersion, ref_id;
				uint32_t ref_time_secs, ref_time_frac;
				uint32_t orig_time_secs, orig_time_frac;
				uint32_t rx_time_secs, rx_time_frac;
				uint32_t tx_time_secs, tx_time_frac;
			} packet;
			packet.li_vn_mode = 0x1b;
			int64_t esp_tx_usec = esp_timer_get_time();
			if (sendto(sock, &packet, sizeof packet, 0, (struct sockaddr *) &dst, dst_size) < 0) {
				ESP_LOGE(TAG " NTP", "Unable to send packet: %s", strerror(errno));
				break;
			}
			if (recvfrom(sock, &packet, sizeof packet, 0, (struct sockaddr *) &src, &src_size) < 0) {
				ESP_LOGE(TAG " NTP", "Unable to receive packet: %s", strerror(errno));
				break;
			}
			int64_t esp_rx_usec = esp_timer_get_time();
			int64_t ntp_rx_secs = ntohl(packet.rx_time_secs) * USECS_TO_SEC + ntohl(packet.rx_time_frac) / USECS_TO_FRAC;
			int64_t ntp_tx_secs = ntohl(packet.tx_time_secs) * USECS_TO_SEC + ntohl(packet.tx_time_frac) / USECS_TO_FRAC;
			int64_t esp_dt_usec = esp_rx_usec - esp_tx_usec;
			int64_t new_ntp_time_usec = (ntp_rx_secs + ntp_tx_secs) / 2 + esp_dt_usec / 2;
			int64_t new_esp_time_usec = esp_rx_usec;

			int64_t difference_usec = (new_ntp_time_usec - new_esp_time_usec) - (ntp_time_usec - esp_time_usec);
			ESP_LOGI(TAG " NTP", "Received new packet. Difference: %f secs", ((double) difference_usec) / USECS_TO_SEC);
			ntp_time_usec = new_ntp_time_usec;
			esp_time_usec = new_esp_time_usec;

			long poll_delay = random(NTP_MIN_POLL_INTERVAL_SECS * MSECS_TO_SEC, NTP_MAX_POLL_INTERVAL_SECS * MSECS_TO_SEC);
			ESP_LOGI(TAG " NTP", "Scheduled next poll in %f secs", ((double) poll_delay) / MSECS_TO_SEC);
			delay(poll_delay);
		}

		if (sock != -1) {
			shutdown(sock, 0);
			close(sock);
		}

		delay(5 * MSECS_TO_SEC);
	}

	vTaskDelete(NULL);
}

static void display_task(void *arg)
{
	while (1) {
		int64_t time_usec = ntp_time_usec + esp_timer_get_time() - esp_time_usec;
		double time_secs = ((double) time_usec) / USECS_TO_SEC;
		double frac_secs = time_secs - ((int64_t) time_secs);
		double poll_secs = ((double) ntp_time_usec) / USECS_TO_SEC;

		display.clearDisplay();
		display.setCursor(0, 0);
		display.printf("Seconds since epoch:\n%f\n\n", time_secs);
		display.printf("Latest NTP packet:\n%f\n", poll_secs);
		display.fillRect(0, display.height() - 3, frac_secs * display.width(), display.height(), SSD1306_WHITE);
		display.display();

		delay(20);
	}
}

extern "C" void app_main()
{
	ESP_LOGI(TAG, "Initialising board...");
	initArduino();
	Wire.begin(SDA, SCL);
	randomSeed(analogRead(0));

	ESP_LOGI(TAG, "Initialising OLED display...");
	if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
		ESP_LOGE(TAG, "SSD1306 allocation failed");
		return;
	}
	display.setTextColor(SSD1306_WHITE);
	display.printf("\nConnecting to %s...", WIFI_SSID);
	display.display();

	ESP_LOGI(TAG, "Connecting to WiFi...");
	WiFi.begin(WIFI_SSID, WIFI_PSWD);
	while (WiFi.status() != WL_CONNECTED) {
		delay(100);
	}

	ESP_LOGI(TAG, "Creating tasks...");
	xTaskCreate(ota_task, "ota", 8096, NULL, 5, NULL);
	xTaskCreate(ntp_task, "ntp", 8096, NULL, 5, NULL);
	xTaskCreate(display_task, "display", 8096, NULL, 5, NULL);
}
