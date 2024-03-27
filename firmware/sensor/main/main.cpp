#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <LoRa.h>
#include <WiFi.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_timer.h>
#include <lwip/netdb.h>

#define TAG "ESP32resso"

#define TASK_STACK_SIZE (8192)
#define TASK_PRIORITY (5)

#define MSECS_TO_SEC (1000ll)
#define USECS_TO_SEC (MSECS_TO_SEC * 1000ll)
#define USECS_TO_FRAC (0xFFFFFFFFull / USECS_TO_SEC)

#define MIC_PIN (13)

#define OLED_SHOW_MSEC (1500 * MSECS_TO_SEC)
#define OLED_POLL_MSEC 50
#define OLED_SDA 4
#define OLED_SCL 15
#define OLED_RST 16
#define OLED_ADDR 0x3c
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_BUTTON_PIN 0

#define NTP_HOST "pool.ntp.org"
#define NTP_PORT (123)
#define NTP_TIMEOUT_SEC (10)
#define NTP_POLL_ERR_SEC (10)
#define NTP_POLL_MIN_SEC (5)
#define NTP_POLL_MAX_SEC (120)
#define NTP_MAX_DIFF_SEC (0.25)

#define SYNC_MSEC (10 * MSECS_TO_SEC)

#define PING_MIN_MSEC (60 * MSECS_TO_SEC)
#define PING_MAX_MSEC (120 * MSECS_TO_SEC)

#define BANG_POLL_MSEC (200)
#define BANG_COOLDOWN_MSEC (500)
#define BANG_THRESHOLD (9999)

#define WIFI_SSID "msungie"
#define WIFI_PSWD "hamandcheese"

// DEBUG: Run `python3 -m http.server` in /firmware/sensor directory with OTA_HOST set to your IP address
#define OTA_HOST "192.168.19.134"
#define OTA_PORT (8000)
#define OTA_PATH "/build/sensor.bin"
#define OTA_CAP (16)

#define LORA_SCK (5)
#define LORA_MISO (19)
#define LORA_MOSI (27)
#define LORA_SS (18)
#define LORA_RST (14)
#define LORA_DIO0 (26)
#define LORA_BAND (868e6)
#define LORA_BANG_HEADER (0xB4E7)
#define LORA_PING_HEADER (0xB4E8)
#define LORA_SYNC_HEADER (0xB4E9)
#define LORA_OTAS_HEADER (0xB4EA)

struct lora_packet { uint16_t header; uint64_t field0, field1; } __attribute__((__packed__));
struct lora_msg { uint64_t usec; struct lora_packet packet; };

static SemaphoreHandle_t lock = NULL; // Mutex for following global state
static QueueHandle_t msg_queue = NULL; // Received LoRa messages yet to be processed
static uint64_t ntp_time_usec = 0, esp_time_usec = 0; // Time represented as #usecs since NTP epoch at #usecs since boot
static bool is_connecting = true; // Are we still trying to connect to WiFi AP
static bool is_gateway = false; // Is this board a gateway (i.e. connected to WiFi)
static uint16_t ota_progress = 0; // How many OTA chunks have been applied since beginning a new OTA update
static uint16_t bangs_detected = 0; // How many bangs has the microphone detected since boot
static QueueHandle_t ota_queue = NULL; // OTA chunks yet to be broadcasted
static uint64_t ota_version = 0; // Firmware version OTA chunks apply to
static const uint64_t ota_term[2] = { 0x1832809, 0x9873412 }; // Special values to designate the end of an OTA
static const esp_partition_t *ota_partition = NULL; // In-progress OTA context
static esp_ota_handle_t ota_handle; // In-progress OTA data context
static uint64_t version = * (uint64_t *) esp_app_get_description()->app_elf_sha256; // Firmware version
static uint64_t chip_id = ESP.getEfuseMac(); // Unique ID for sensor

// Stub function for various timer timeouts
static void timeout_stub(TimerHandle_t timer) {}

// Upload bang timestamp to cloud
static void bang_upload(uint64_t id, uint64_t usec)
{
	ESP_LOGE(TAG " BANG", "Uploading bang timestamp to cloud: POST id=%llX time=%llu", id, usec);
	// TODO(http upload)
}

// Send a LoRa packet
static void lora_write(struct lora_packet packet)
{
	LoRa.beginPacket(true);
	LoRa.write((uint8_t *) &packet, sizeof packet);
	LoRa.endPacket(true);
}

// LoRa receive packet handler
void lora_read(int packet_len)
{
	uint64_t now_usec = esp_timer_get_time();
	xQueueSendFromISR(msg_queue, &now_usec, NULL);
}

// Apply OTA chunk
static void ota_write(uint64_t *ota, int ota_len)
{
	esp_err_t err;
	bool is_finished = false;

	if (!ota_partition) {
		ESP_LOGE(TAG " OTA", "Updating firmware...");
		if (!(ota_partition = esp_ota_get_next_update_partition(NULL))) {
			ESP_LOGE(TAG " OTA", "Unable to find valid OTA partition to use");
			goto end0;
		}
		LoRa.idle(); // hack to fix lora library mishandling interrupts while flash is occupied
		err = esp_ota_begin(ota_partition, OTA_SIZE_UNKNOWN, &ota_handle);
		LoRa.receive(sizeof (struct lora_packet));
		if (err != ESP_OK) {
			ESP_LOGE(TAG " OTA", "Unable to start OTA");
			goto end0;
		}
		ota_progress = 0;
	}

	if (ota_len >= 2 && ota[ota_len - 2] == ota_term[0] && ota[ota_len - 1] == ota_term[1]) {
		ESP_LOGI(TAG " OTA", "OTA update marked as complete.");
		is_finished = true;
		ota_len -= 2;
	}

	if (ota_len) {
		ESP_LOGW(TAG " OTA", "Writing OTA chunk...");
		// TODO(future): delta updates
		if ((err = esp_ota_write(ota_handle, ota, ota_len * sizeof *ota)) != ESP_OK) {
			ESP_LOGE(TAG " OTA", "Unable to write OTA chunk");
			goto end1;
		}
		if (false) goto end1;
		ota_progress += ota_len;
	}

	if (is_finished) {
		ESP_LOGE(TAG " OTA", "OTA update complete.");
		if ((err = esp_ota_end(ota_handle)) != ESP_OK) {
			ESP_LOGE(TAG " OTA", "Unable to validate OTA");
			goto end0;
		}
		if ((err = esp_ota_set_boot_partition(ota_partition)) != ESP_OK) {
			ESP_LOGE(TAG " OTA", "Unable to set OTA boot partition");
			goto end0;
		}
		ESP_LOGE(TAG " OTA", "Rebooting into new firmware...");
		esp_restart();
	}

	return;

end1:
	if ((err = esp_ota_abort(ota_handle)) != ESP_OK)
		ESP_LOGE(TAG " OTA", "Unable to gracefully abort OTA");
end0:
	ota_partition = NULL;
	ota_progress = 0;
}

// Show info on OLED display on button press
static void oled_task(void *arg)
{
	ESP_LOGW(TAG " OLED", "Starting OLED display task...");
	Adafruit_SSD1306 *oled = NULL;
	TimerHandle_t timer = xTimerCreate("oled", OLED_SHOW_MSEC / portTICK_PERIOD_MS, false, NULL, timeout_stub);
	xTimerStart(timer, portMAX_DELAY);
	pinMode(OLED_BUTTON_PIN, INPUT_PULLUP);

	while (true) {
		if (digitalRead(OLED_BUTTON_PIN) == 0) {
			ESP_LOGI(TAG " OLED", "Button down, resetting timer...");
			xTimerReset(timer, portMAX_DELAY);
		}

		if (xTimerIsTimerActive(timer) && !oled) {
			ESP_LOGW(TAG " OLED", "Enabling display...");
			Wire.begin(OLED_SDA, OLED_SCL);
			oled = new Adafruit_SSD1306(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RST);
			oled->begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
			oled->setTextColor(SSD1306_WHITE);
			digitalWrite(OLED_RST, HIGH);
		} else if (!xTimerIsTimerActive(timer) && oled) {
			ESP_LOGW(TAG " OLED", "Disabling display...");
			digitalWrite(OLED_RST, LOW);
			delete oled;
			oled = NULL;
			Wire.end();
		}

		if (oled) {
			oled->clearDisplay();
			oled->setCursor(0, 0);
			xSemaphoreTake(lock, portMAX_DELAY);

			oled->printf("ID: %llX\n", chip_id);
			oled->printf("Ver: %llX\n", version);
			oled->printf("\n");

			if (is_connecting) {
				oled->printf("Connecting to " WIFI_SSID "...");
			} else {
				oled->printf("WiFi: %.16s\n", is_gateway ? WIFI_SSID : "none");
				oled->printf("OTAs: %u\n", ota_progress);
				oled->printf("Sensor: %u (%d)\n", analogRead(MIC_PIN), bangs_detected);
				oled->printf("=> ");
				if (esp_time_usec) {
					uint64_t now = ntp_time_usec + esp_timer_get_time() - esp_time_usec;
					double time_sec = now / (double) USECS_TO_SEC;
					double frac_sec = time_sec - (int64_t) time_sec;
					oled->printf("%f\n", time_sec);
					oled->fillRect(0, oled->height() - 3, frac_sec * oled->width(), oled->height(), SSD1306_WHITE);
				} else {
					oled->printf("no sync\n");
				}
			}

			xSemaphoreGive(lock);
			oled->display();
		}

		delay(OLED_POLL_MSEC);
	}
}

// Detect bangs and push towards cloud
static void bang_task(void *arg)
{
	ESP_LOGW(TAG " BANG", "Starting bang task...");

	TimerHandle_t timer = xTimerCreate("mic_cooldown", BANG_COOLDOWN_MSEC / portTICK_PERIOD_MS, false, NULL, timeout_stub);
	while (true) {
		// TODO(mic)
		// if (!xTimerIsTimerActive(timer) && analogRead(MIC_PIN) > BANG_THRESHOLD) {
		// if (!xTimerIsTimerActive(timer) && random(200) == 0) {
		if (false) {
			uint64_t now_usec = esp_timer_get_time();
			ESP_LOGW(TAG " BANG", "Microphone bang threshold triggered!");
			xTimerReset(timer, portMAX_DELAY);

			xSemaphoreTake(lock, portMAX_DELAY);
			bangs_detected += 1;
			if (is_gateway) {
				ESP_LOGI(TAG " BANG", "Uploading local bang to cloud...");
				bang_upload(chip_id, now_usec);
			} else {
				ESP_LOGI(TAG " BANG", "Transmitting bang to gateway...");
				LoRa.idle();
				lora_write((struct lora_packet) { LORA_BANG_HEADER, chip_id, ntp_time_usec + now_usec - esp_time_usec });
				LoRa.receive(sizeof (struct lora_packet));
			}
			xSemaphoreGive(lock);
		}

		delay(BANG_POLL_MSEC);
	}
}

// Occasionally broadcast ping to gateway or query cloud for updates
static void ping_task(void *arg)
{
	ESP_LOGW(TAG " PING", "Starting ping task...");

	while (true) {
		xSemaphoreTake(lock, portMAX_DELAY);
		ESP_LOGW(TAG " PING", "Performing occasional ping...");
		if (is_gateway) {
			if (!ota_version) {
				ESP_LOGI(TAG " PING", "Flagging current version for OTA consideration...");
				ota_version = version;
			}
		} else {
			ESP_LOGI(TAG " PING", "Transmitting version ping to gateway...");
			LoRa.idle();
			lora_write((struct lora_packet) { LORA_PING_HEADER, chip_id, version });
			LoRa.receive(sizeof (struct lora_packet));
		}
		xSemaphoreGive(lock);
		delay(random(PING_MIN_MSEC, PING_MAX_MSEC));
	}
}

// Fetch OTA chunks from cloud for ota_version and push to ota_queue
static void gateway_get_task(void *arg)
{
	ESP_LOGW(TAG " GET", "Starting get task...");

	while (true) {

		ESP_LOGI(TAG " GET", "Waiting for version for OTA consideration...");
		while (!ota_version)
			delay(1 * MSECS_TO_SEC);


		ESP_LOGI(TAG " GET", "Querying OTA for version %llX...", ota_version);
		esp_err_t err;
		esp_http_client_config_t http_config = {};
		esp_http_client_handle_t http_handle;
		int64_t content_len = 0, read_len = 0;
		int status_code;
		http_config.host = OTA_HOST;
		http_config.port = OTA_PORT;
		http_config.path = OTA_PATH;
		if (!(http_handle = esp_http_client_init(&http_config))) {
			ESP_LOGE(TAG " GET", "Unable to create HTTP client");
			goto end0;
		}
		if ((err = esp_http_client_open(http_handle, 0)) != ESP_OK) {
			ESP_LOGE(TAG " GET", "Unable to open HTTP connection");
			goto end1;
		}
		if ((content_len = esp_http_client_fetch_headers(http_handle)) < 0) {
			ESP_LOGE(TAG " GET", "Unable to read HTTP headers");
			goto end1;
		}
		if ((status_code = esp_http_client_get_status_code(http_handle)) != 200) {
			ESP_LOGE(TAG " GET", "Received non-200 status code: %d", status_code);
			goto end1;
		}
		if (content_len == 0) {
			ESP_LOGI(TAG " GET", "No OTA for version %llX received", ota_version);
			goto end1;
		}

		ESP_LOGW(TAG " GET", "Streaming OTA for version %llX via sync windows...", ota_version);
		while (!esp_http_client_is_complete_data_received(http_handle)) {
			int64_t len = 0, ota_len = 0;
			uint64_t ota[128] = {};
			if ((len = esp_http_client_read(http_handle, (char *) ota, sizeof ota)) < 0) {
				ESP_LOGE(TAG " GET", "Unable to read HTTP data");
				goto end1;
			}
			read_len += len;
			ota_len = (len + sizeof (int64_t) - 1) / sizeof (int64_t);
			ESP_LOGI(TAG " GET", "New OTA chunks: %lld (%lld bytes, %lld/%lld)", ota_len, len, read_len, content_len);
			for (int i = 0; i < ota_len; i += 2)
				xQueueSend(ota_queue, ota + i, portMAX_DELAY);
		}

		ESP_LOGW(TAG " GET", "Streaming of OTA for version %llX complete.", ota_version);
		xQueueSend(ota_queue, ota_term, portMAX_DELAY);
end1:
		if ((err = esp_http_client_cleanup(http_handle)) != ESP_OK)
			ESP_LOGE(TAG " GET", "Unable to gracefully cleanup HTTP client");
end0:
		while (uxQueueMessagesWaiting(ota_queue))
			delay(1 * MSECS_TO_SEC);

		xSemaphoreTake(lock, portMAX_DELAY);
		ESP_LOGI(TAG " GET", "OTA done for version %llX complete.", ota_version);
		ota_version = 0;
		xSemaphoreGive(lock);
	}
}

// Occasionally query NTP public pool for current time
static void gateway_ntp_task(void *arg)
{
	ESP_LOGW(TAG " NTP", "Starting NTP task...");

	struct ntp_packet {
		uint8_t li_vn_mode, stratum, poll, precision;
		uint32_t root_delay, root_dispersion, ref_id;
		uint32_t ref_time_secs, ref_time_frac;
		uint32_t orig_time_secs, orig_time_frac;
		uint32_t rx_time_secs, rx_time_frac;
		uint32_t tx_time_secs, tx_time_frac;
	};

	while (true) {
		int delay_msec = NTP_POLL_ERR_SEC * MSECS_TO_SEC;

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

		ESP_LOGI(TAG " NTP", "Sending NTP request packet...");

		if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) < 0) {
			ESP_LOGE(TAG " NTP", "Unable to create new socket: %s", strerror(errno));
			goto end0;
		}
		timeout.tv_sec = NTP_TIMEOUT_SEC;
		if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout) < 0) {
			ESP_LOGE(TAG " NTP", "Unable to configure socket: %s", strerror(errno));
			goto end1;
		}

		if (!(host = gethostbyname(NTP_HOST)) || !host->h_addr) {
			ESP_LOGE(TAG " NTP", "Unable to get IP address of " NTP_HOST);
			goto end1;
		}
		dst.sin_family = AF_INET;
		dst.sin_addr = *((struct in_addr *) host->h_addr);
		dst.sin_port = htons(NTP_PORT);
		bzero(&dst.sin_zero, 8);

		packet.li_vn_mode = 0x1b;
		esp_tx_usec = esp_timer_get_time();
		if (sendto(sock, &packet, sizeof packet, 0, (struct sockaddr *) &dst, sizeof dst) < 0) {
			ESP_LOGE(TAG " NTP", "Unable to send request packet: %s", strerror(errno));
			goto end1;
		}
		if (recvfrom(sock, &packet, sizeof packet, 0, (struct sockaddr *) &src, &src_size) < 0) {
			ESP_LOGE(TAG " NTP", "Unable to receive response packet: %s", strerror(errno));
			goto end1;
		}
		esp_rx_usec = esp_timer_get_time();

		// Given the absolute time the server received, the absolute time the server responded and
		// the total time the exchange took, approximate the absolute time the client received
		ntp_rx_secs = ntohl(packet.rx_time_secs) * USECS_TO_SEC + ntohl(packet.rx_time_frac) / USECS_TO_FRAC;
		ntp_tx_secs = ntohl(packet.tx_time_secs) * USECS_TO_SEC + ntohl(packet.tx_time_frac) / USECS_TO_FRAC;
		new_ntp_time_usec = (ntp_rx_secs + ntp_tx_secs) / 2 + (esp_rx_usec - esp_tx_usec) / 2;
		new_esp_time_usec = esp_rx_usec;
		xSemaphoreTake(lock, portMAX_DELAY);
		time_diff_sec = ((int64_t) (new_ntp_time_usec - new_esp_time_usec) - (int64_t) (ntp_time_usec - esp_time_usec)) / (double) USECS_TO_SEC;
		ntp_time_usec = new_ntp_time_usec;
		esp_time_usec = new_esp_time_usec;
		xSemaphoreGive(lock);

		ESP_LOGW(TAG " NTP", "Received NTP response packet from %s - Time difference: %f secs", inet_ntoa(dst.sin_addr), time_diff_sec);

		// The next NTP request should be scheduled sooner if there was a large change in time
		delay_msec = MSECS_TO_SEC - constrain(abs(time_diff_sec / NTP_MAX_DIFF_SEC), 0, 1) * MSECS_TO_SEC;
		delay_msec = NTP_POLL_MIN_SEC * MSECS_TO_SEC + (NTP_POLL_MAX_SEC - NTP_POLL_MIN_SEC) * delay_msec;

end1:
		if (close(sock) < 0)
			ESP_LOGE(TAG " NTP", "Unable to close socket: %s", strerror(errno));
end0:
		ESP_LOGI(TAG " NTP", "Scheduled next NTP query: %f secs", delay_msec / (double) MSECS_TO_SEC);
		delay(delay_msec);
	}
}

// Regularly broadcast current time and queued OTAs over LoRa
static void gateway_sync_task(void *arg)
{
	ESP_LOGW(TAG " SYNC", "Starting sync task...");

	while (true) {
		xSemaphoreTake(lock, portMAX_DELAY);
		ESP_LOGW(TAG " SYNC", "Broadcasting sync...");

		int ota_len = 0;
		uint64_t ota[OTA_CAP*2]; // 64bit OTA chunks are transmitted in pairs, so each queue entry is 128bit
		for (; ota_len < OTA_CAP*2 && xQueueReceive(ota_queue, ota + ota_len, 0); ota_len += 2);

		LoRa.idle();
		ESP_LOGI(TAG " SYNC", "Transmitting sync packet...");
		lora_write((struct lora_packet) { LORA_SYNC_HEADER, ntp_time_usec + esp_timer_get_time() - esp_time_usec, ota_version });
		if (ota_len)
			ESP_LOGI(TAG " SYNC", "Piggybacking %d OTA packets for version %llX...", ota_len, ota_version);
		for (int i = 0; i < ota_len; i += 2) {
			delay(50);
			lora_write((struct lora_packet) { LORA_OTAS_HEADER, ota[i+0], ota[i+1] });
		}
		LoRa.receive(sizeof (struct lora_packet));

		if (ota_version == version)
			ota_write(ota, ota_len);

		xSemaphoreGive(lock);
		delay(SYNC_MSEC);
	}
}

// Handle LoRa packets received by gateway
static void gateway_lora_task(void *arg)
{
	while (true) {
		struct lora_msg msg;
		xQueueReceive(msg_queue, &msg.usec, portMAX_DELAY);
		LoRa.readBytes((uint8_t *) &msg.packet, sizeof msg.packet);

		xSemaphoreTake(lock, portMAX_DELAY);
		if (msg.packet.header == LORA_PING_HEADER) {
			ESP_LOGI(TAG " LORA", "Received ping packet: id=%llu ver=%llX", msg.packet.field0, msg.packet.field1);
			if (!ota_version) {
				ESP_LOGI(TAG " PING", "Flagging ping version for OTA consideration...");
				ota_version = msg.packet.field1;
			}
		} else if (msg.packet.header == LORA_BANG_HEADER) {
			ESP_LOGI(TAG " LORA", "Received bang packet: id=%llu usec=%llX", msg.packet.field0, msg.packet.field1);
			bang_upload(msg.packet.field0, msg.packet.field1);
		}
		xSemaphoreGive(lock);
	}
}

// Handle LoRa packets received by remote
static void remote_lora_task(void *arg)
{
	while (true) {
		struct lora_msg msg;
		xQueueReceive(msg_queue, &msg.usec, portMAX_DELAY);
		LoRa.readBytes((uint8_t *) &msg.packet, sizeof msg.packet);

		xSemaphoreTake(lock, portMAX_DELAY);
		if (msg.packet.header == LORA_SYNC_HEADER) {
			ntp_time_usec = msg.packet.field0;
			esp_time_usec = msg.usec;
			ota_version = msg.packet.field1;
			ESP_LOGI(TAG " LORA", "Received sync packet: usec=%llu ota=%llX", msg.packet.field0, msg.packet.field1);
		} else if (msg.packet.header == LORA_OTAS_HEADER) {
			ESP_LOGI(TAG " LORA", "Received OTA packet: %llX%llX", msg.packet.field0, msg.packet.field1);
			if (ota_version == version) {
				uint64_t ota[2] = { msg.packet.field0, msg.packet.field1 };
				ota_write(ota, 2);
			}
		}
		xSemaphoreGive(lock);
	}
}

// Entry point
extern "C" void app_main()
{
	ESP_LOGW(TAG, "Initialising framework...");
	initArduino();
	Serial.begin(115200);
	randomSeed(analogRead(0));

	ESP_LOGW(TAG, "Initialising application...");
	lock = xSemaphoreCreateMutex(),
	msg_queue = xQueueCreate(OTA_CAP, sizeof (uint64_t));
	ota_queue = xQueueCreate(OTA_CAP, 2 * sizeof (uint64_t));
	xTaskCreate(oled_task, "oled", TASK_STACK_SIZE, NULL, TASK_PRIORITY, NULL);

	ESP_LOGW(TAG, "Attempting to connecting to local WiFi AP...");
	if (chip_id == 0x5c9a66f23a08) WiFi.begin(WIFI_SSID, WIFI_PSWD); // TODO(clean): hardcoded
	for (int i = 0; WiFi.status() != WL_CONNECTED && i < 10; i++)
		delay(1 * MSECS_TO_SEC);

	xSemaphoreTake(lock, portMAX_DELAY);
	is_connecting = false;
	is_gateway = (WiFi.status() == WL_CONNECTED);
	SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
	LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
	LoRa.begin(LORA_BAND);
	// TODO(lora): lora params
	LoRa.onReceive(lora_read);
	LoRa.receive(sizeof (struct lora_packet));
	if (is_gateway) {
		ESP_LOGE(TAG, "WiFi connection established, performing as gateway sensor device...");
		xTaskCreate(gateway_ntp_task, "ntp", TASK_STACK_SIZE, NULL, TASK_PRIORITY, NULL);
		xTaskCreate(gateway_get_task, "get", TASK_STACK_SIZE, NULL, TASK_PRIORITY, NULL);
		xTaskCreate(gateway_lora_task, "lora", TASK_STACK_SIZE, NULL, TASK_PRIORITY, NULL);
	} else {
		ESP_LOGE(TAG, "WiFi not connected, performing as remote sensor device...");
		xTaskCreate(remote_lora_task, "lora", TASK_STACK_SIZE, NULL, TASK_PRIORITY, NULL);
		WiFi.mode(WIFI_OFF);
	}
	xSemaphoreGive(lock);

	ESP_LOGW(TAG, "Waiting for first time sync before starting bang detection...");
	while (!ntp_time_usec)
		delay(1 * MSECS_TO_SEC);

	ESP_LOGW(TAG, "Time synchronised, starting bang detection...");
	xTaskCreate(bang_task, "bang", TASK_STACK_SIZE, NULL, TASK_PRIORITY, NULL);
	xTaskCreate(ping_task, "ping", TASK_STACK_SIZE, NULL, TASK_PRIORITY, NULL);
	if (is_gateway) {
		xTaskCreate(gateway_sync_task, "sync", TASK_STACK_SIZE, NULL, TASK_PRIORITY, NULL);
	}
}
