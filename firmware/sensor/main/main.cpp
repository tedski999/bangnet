#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <LoRa.h>
#include <WiFi.h>
#include <arduinoFFT.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_timer.h>
#include <lwip/netdb.h>
extern "C" {
#include <detools.h>
}

#define TASK_STACK_SIZE (8192)
#define TASK_PRIORITY (5)

#define MSECS_TO_SEC (1000ll)
#define USECS_TO_SEC (MSECS_TO_SEC * 1000ll)
#define USECS_TO_FRAC (0xFFFFFFFFull / USECS_TO_SEC)

#define MIC_PIN (13)
#define MIC_SAMPLES (512) //must be 2^x number
#define MIC_SAMPLE_FREQ (512.0) //2x the expected frequency
#define MIC_PEAK_SAMPLES (8)
#define MIC_PEAK_THRESHOLD (1.4)
#define MIC_COOLDOWN_MSEC (500)

#define OLED_SHOW_MSEC (300 * MSECS_TO_SEC)
#define OLED_POLL_MSEC (50)
#define OLED_SDA (4)
#define OLED_SCL (15)
#define OLED_RST (16)
#define OLED_ADDR (0x3c)
#define OLED_WIDTH (128)
#define OLED_HEIGHT (64)
#define OLED_CHAR_WIDTH (6)
#define OLED_CHAR_HEIGHT (8)
#define OLED_TEXT_WIDTH (OLED_WIDTH / OLED_CHAR_WIDTH)
#define OLED_TEXT_HEIGHT (OLED_HEIGHT / OLED_CHAR_HEIGHT)
#define OLED_BUF_WIDTH (OLED_TEXT_WIDTH * 2)
#define OLED_BUF_HEIGHT (OLED_TEXT_HEIGHT * 1)
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

#define WIFI_SSID "msungie"
#define WIFI_PSWD "hamandcheese"

#define CLOUD_HOST "34.241.234.234"
#define CLOUD_PORT (8000)

#define LORA_SCK (5)
#define LORA_MISO (19)
#define LORA_MOSI (27)
#define LORA_SS (18)
#define LORA_RST (14)
#define LORA_DIO0 (26)
#define LORA_BAND (868e6)
#define LORA_CODING_RATE_4 (8)
#define LORA_SPREADING_FACTOR (6)
#define LORA_BURST_CAP (32)
#define LORA_BANG_HEADER (0xB4E7)
#define LORA_PING_HEADER (0xB4E8)
#define LORA_SYNC_HEADER (0xB4E9)
#define LORA_OTAS_HEADER (0xB4EA)

#define MIN(a, b) (a < b ? a : b)

typedef uint64_t lora_payload_t[2];
struct lora_packet { uint16_t header; lora_payload_t payload; } __attribute__((__packed__));
struct lora_msg { uint64_t usec; struct lora_packet packet; };

static SemaphoreHandle_t lock = NULL; // Mutex for following global state
static QueueHandle_t msg_queue = NULL; // Received LoRa messages yet to be processed
static uint64_t ntp_time_usec = 0, esp_time_usec = 0; // Time represented as #usecs since NTP epoch at #usecs since boot
static bool is_connecting = true; // Are we still trying to connect to WiFi AP
static bool is_gateway = false; // Is this board a gateway (i.e. connected to WiFi)
static uint16_t bangs_detected = 0; // How many bangs has the microphone detected since boot
static double mic_image[OLED_WIDTH] = {}; // FFT visualisation
static double mic_peak_value = 0; // FFT visualisation
static QueueHandle_t ota_queue = NULL; // OTA chunks yet to be broadcasted
static const esp_partition_t *ota_new_partition = NULL, *ota_old_partition = NULL; // In-progress OTA context
static esp_ota_handle_t ota_handle; // In-progress OTA data context
static struct detools_apply_patch_t patch; // In-progress OTA delta context
static uintptr_t ota_mem_idx = 0; // OTA delta patch seek address
static uint64_t ota_len = 0, ota_idx = 0; // Bytes in and that have been applied in OTA update
static uint64_t ota_version = 0; // Firmware version OTA chunks apply to
static uint64_t version = * (uint64_t *) esp_app_get_description()->app_elf_sha256; // Firmware version
static uint64_t chip_id = ESP.getEfuseMac(); // Unique ID for sensor
static uint8_t oled_screen = 0; // Show system status, device logs or sensor information
static vprintf_like_t log_handler = NULL; // Original ESP32 log handler
static char log_buf[OLED_BUF_WIDTH * OLED_BUF_HEIGHT] = {}; // Space to store device logs
static int log_buf_line = 0; // Circular buffer index for log_buf

static double mic_peak_record[MIC_PEAK_SAMPLES];
static byte mic_peak_record_i = 0;
static double reals[MIC_SAMPLES] = {};
static double imags[MIC_SAMPLES] = {};

// Stub function for various timer timeouts
static void timeout_stub(TimerHandle_t timer) {}

// Upload bang timestamp to cloud
static void bang_upload(uint64_t id, uint64_t usec)
{
	char path[256];
	esp_err_t err;
	esp_http_client_config_t http_config = {};
	esp_http_client_handle_t http_handle;
	int status_code;

	ESP_LOGW("$BANG", "Uploading bang timestamp to cloud: POST id=%llu time=%llu", id, usec);
	snprintf(path, sizeof path, "/api/micro_record/?micro_number=%llu&bang_time=%llu", id, usec);
	http_config.host = CLOUD_HOST;
	http_config.port = CLOUD_PORT;
	http_config.path = path;
	if (!(http_handle = esp_http_client_init(&http_config))) {
		ESP_LOGE("$BANG", "Unable to create HTTP client");
		goto end0;
	}
	if ((err = esp_http_client_open(http_handle, 0)) != ESP_OK) {
		ESP_LOGE("$BANG", "Unable to open HTTP connection");
		goto end1;
	}
	if (esp_http_client_fetch_headers(http_handle) < 0) {
		ESP_LOGE("$BANG", "Unable to read HTTP headers");
		goto end1;
	}
	if ((status_code = esp_http_client_get_status_code(http_handle)) != 200) {
		ESP_LOGE("$BANG", "Received non-200 status code: %d", status_code);
		goto end1;
	}
end1:
	esp_http_client_cleanup(http_handle);
end0:
	return;
}

// Send a LoRa packet
static void lora_write(struct lora_packet packet)
{
	LoRa.beginPacket(true);
	LoRa.write((uint8_t *) &packet, sizeof packet);
	LoRa.endPacket(false);
}

// LoRa receive packet handler
void lora_read(int packet_len)
{
	struct lora_msg msg;
	msg.usec = esp_timer_get_time();
	LoRa.readBytes((uint8_t *) &msg.packet, sizeof msg.packet);
	xQueueSendFromISR(msg_queue, &msg, NULL);
}

// Callback functions for ota_write
static int ota_mem_read(void *arg, uint8_t *buf, size_t len) { return esp_partition_read(ota_old_partition, ota_mem_idx, buf, len); }
static int ota_mem_seek(void *arg, int offset) { ota_mem_idx += offset; return 0; }
static int ota_mem_write(void *arg, const uint8_t *buf, size_t len) { return esp_ota_write(ota_handle, buf, len) || ota_mem_seek(arg, len); }

// Apply OTA chunk
static void ota_write(lora_payload_t *ota_chunks, int ota_chunks_len)
{
	int err;
	int len;

	LoRa.idle(); // hack to fix lora library mishandling interrupts while flash is occupied

	if (!ota_new_partition) {
		ota_len = ota_chunks[0][0];
		ota_idx = ota_chunks[0][1];
		if (ota_len == 0 || ota_len > (1ll << 32) || ota_idx != 0) {
			ESP_LOGD("$OTA", "Ignoring malformed OTA chunks");
			goto end1;
		}
		ota_mem_idx = 0;
		ota_chunks += 1;
		ota_chunks_len -= 1;
		ESP_LOGW("$OTA", "Updating firmware (%llu byte patch)...", ota_len);
		if (!(ota_new_partition = esp_ota_get_next_update_partition(NULL))) {
			ESP_LOGE("$OTA", "Unable to find valid OTA partition to use");
			goto end1;
		}
		if (!(ota_old_partition = esp_ota_get_running_partition())) {
			ESP_LOGE("$OTA", "Unable to find current OTA partition in use");
			goto end1;
		}
		err = esp_ota_begin(ota_new_partition, OTA_SIZE_UNKNOWN, &ota_handle);
		if (err != ESP_OK) {
			ESP_LOGE("$OTA", "Unable to start OTA: err %s (err %d)", esp_err_to_name(err), err);
			goto end1;
		}
		if ((err = detools_apply_patch_init(&patch, ota_mem_read, ota_mem_seek, ota_len, ota_mem_write, NULL))) {
			ESP_LOGE("$OTA", "Unable to init OTA: err %d", err);
			goto end2;
		}
	}

	len = MIN(ota_chunks_len * sizeof *ota_chunks, ota_len - ota_idx);
	ota_idx += len;

	ESP_LOGI("$OTA", "Writing OTA chunk (%llu / %llu)...", ota_idx, ota_len);
	err = detools_apply_patch_process(&patch, (uint8_t *) *ota_chunks, len);
	if (err) {
		ESP_LOGE("$OTA", "Unable to patch OTA chunk: err %d", err);
		goto end3;
	}

	if (ota_idx < ota_len)
		goto end0;

end3:
	if ((err = detools_apply_patch_finalize(&patch)) < 0) {
		ESP_LOGE("$OTA", "Unable to complete OTA patch: err %d", err);
	} else if (ota_idx >= ota_len) {
		ESP_LOGW("$OTA", "OTA patch complete.");
		if ((err = esp_ota_end(ota_handle)) != ESP_OK) {
			ESP_LOGE("$OTA", "Unable to complete OTA: %s (err %d)", esp_err_to_name(err), err);
			goto end1;
		}
		if ((err = esp_ota_set_boot_partition(ota_new_partition)) != ESP_OK) {
			ESP_LOGE("$OTA", "Unable to set OTA boot partition: %s (err %d)", esp_err_to_name(err), err);
			goto end1;
		}
		ESP_LOGW("$OTA", "Rebooting into new firmware...");
		delay(5 * MSECS_TO_SEC);
		esp_restart();
	}
end2:
	if ((err = esp_ota_abort(ota_handle)) != ESP_OK)
		ESP_LOGE("$OTA", "Unable to gracefully abort OTA: err %d", err);
end1:
	ota_new_partition = NULL;
	ota_version = 0;
end0:
	LoRa.receive(sizeof (struct lora_packet));
}

// Write device logs to circular buffer
int log_handler_wrapper(const char *format, va_list ap)
{
	char buf[128];
	va_list ap_copy;
	va_copy(ap_copy, ap);
	vsnprintf(buf, 128, format, ap);
	va_end(ap_copy);
	char *log = strchr(buf, '$');
	if (log) {
		strncpy(log_buf + log_buf_line * OLED_BUF_WIDTH, log, OLED_TEXT_WIDTH);
		log_buf_line = (log_buf_line + 1) % OLED_BUF_HEIGHT;
	}
	return log_handler(format, ap);
}

// Show info on OLED display on button press
static void oled_task(void *arg)
{
	ESP_LOGI("$OLED", "Starting OLED display task...");
	Adafruit_SSD1306 *oled = NULL;
	TimerHandle_t timer = xTimerCreate("oled", OLED_SHOW_MSEC / portTICK_PERIOD_MS, false, NULL, timeout_stub);
	xTimerStart(timer, portMAX_DELAY);
	pinMode(OLED_BUTTON_PIN, INPUT_PULLUP);
	bool is_held = false;

	while (true) {
		bool is_down = digitalRead(OLED_BUTTON_PIN) == 0;
		if (is_down && !is_held) {
			ESP_LOGD("$OLED", "Button down, resetting timer...");
			if (xTimerIsTimerActive(timer)) {
				oled_screen = (oled_screen + 1) % 3;
				ESP_LOGD("$OLED", "Screen set to %d", oled_screen);
			}
			xTimerReset(timer, portMAX_DELAY);
			is_held = true;
		} else if (!is_down) {
			is_held = false;
		}

		if (xTimerIsTimerActive(timer) && !oled) {
			ESP_LOGI("$OLED", "Enabling display...");
			Wire.begin(OLED_SDA, OLED_SCL);
			oled = new Adafruit_SSD1306(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RST);
			oled->begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
			oled->setTextColor(SSD1306_WHITE);
			digitalWrite(OLED_RST, HIGH);
		} else if (!xTimerIsTimerActive(timer) && oled) {
			ESP_LOGI("$OLED", "Disabling display...");
			digitalWrite(OLED_RST, LOW);
			delete oled;
			oled = NULL;
			Wire.end();
		}

		if (oled) {
			oled->clearDisplay();
			oled->setCursor(0, 0);
			xSemaphoreTake(lock, portMAX_DELAY);

			if (oled_screen == 2) {
				if (mic_peak_value == 0) {
					oled->printf("Sound is 0!");
				} else {
					for (int i = 0; i < OLED_WIDTH; i++) {
						oled->drawPixel(i, (mic_image[i] / mic_peak_value) * OLED_HEIGHT, SSD1306_WHITE);
					}
				}
			} else if (oled_screen == 1) {
				for (int i = 0, line = 0; i < OLED_BUF_HEIGHT && line < OLED_BUF_HEIGHT; i++) {
					char *log = strchr(log_buf + ((log_buf_line + i) % OLED_BUF_HEIGHT) * OLED_BUF_WIDTH, '$');
					if (log) {
						oled->setCursor(0, line++ * OLED_CHAR_HEIGHT);
						oled->printf("%.*s", OLED_TEXT_WIDTH, log + 1);
					}
				}
			} else {
				oled->printf("ID: %llX\n", chip_id);
				oled->printf("Ver: %llX\n\n", version);
				if (is_connecting)
					oled->printf("WiFi: ...\n");
				else
					oled->printf("WiFi: %.16s\n", is_gateway ? WIFI_SSID : "none");
				oled->printf("Sound: %d (%d)\n", analogRead(MIC_PIN), bangs_detected);
				oled->printf("=> ");
				if (esp_time_usec == 0) {
					oled->printf("not synced\n");
				} else {
					uint64_t now = ntp_time_usec + esp_timer_get_time() - esp_time_usec;
					double time_sec = now / (double) USECS_TO_SEC;
					double frac_sec = time_sec - (int64_t) time_sec;
					oled->printf("%f\n", time_sec);
					oled->fillRect(0, oled->height() - 3, frac_sec * oled->width(), oled->height(), SSD1306_WHITE);
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
	ESP_LOGI("$BANG", "Starting bang task...");

	ArduinoFFT fft = ArduinoFFT(reals, imags, MIC_SAMPLES, MIC_SAMPLE_FREQ);

	TimerHandle_t timer = xTimerCreate("mic_cooldown", MIC_COOLDOWN_MSEC / portTICK_PERIOD_MS, false, NULL, timeout_stub);

	while (true) {
		unsigned long microSeconds;

		for(int i = 0; i < MIC_SAMPLES; i++) {
			microSeconds = micros();

			reals[i] = analogRead(MIC_PIN);
			imags[i] = 0;

			delay(1);
			while(micros() < (microSeconds + (1000 * MSECS_TO_SEC) / MIC_SAMPLE_FREQ)) {
			}
		}

		fft.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
		fft.compute(FFT_FORWARD);
		fft.complexToMagnitude();
		// double peak = fft.majorPeak();

		int step = (MIC_SAMPLES/2) / OLED_WIDTH;
		int c = 0;
		for (int i = 0; i < (MIC_SAMPLES/2); i+=step) {
			mic_image[c] = 0;
			for (int k = 0; k < step; k++)
				mic_image[c] = mic_image[c] + reals[i+k];
			mic_image[c] = mic_image[c] / step;
			c++;
		}

		double some_average = 0;
		for (int i = 1; i < OLED_WIDTH; i++) {
			mic_peak_value = max(mic_peak_value, mic_image[i]);
			// mic_peak_value = 1000;
			some_average += mic_image[i]/OLED_WIDTH;
		}
		mic_peak_record_i = (mic_peak_record_i+1) % MIC_PEAK_SAMPLES;
		mic_peak_record[mic_peak_record_i] = some_average;
		//mic_peak_record[mic_peak_record_i] = mic_peak_value;
		mic_peak_value = 2000;

		double average = 0;
		bool hasZero = false;
		for (int i = 0; i < MIC_PEAK_SAMPLES; i++) {
			if (i != mic_peak_record_i)
				average += mic_peak_record[i];
			if (mic_peak_record[i] == 0)
				hasZero = true;
		}
		average = average / (MIC_PEAK_SAMPLES-1);

		if (!xTimerIsTimerActive(timer) && !hasZero && mic_peak_record[mic_peak_record_i] > average*MIC_PEAK_THRESHOLD) {
			uint64_t now_usec = ntp_time_usec + esp_timer_get_time() - esp_time_usec;
			ESP_LOGW("$BANG", "Microphone bang threshold triggered!");
			xTimerReset(timer, portMAX_DELAY);

			xSemaphoreTake(lock, portMAX_DELAY);
			bangs_detected += 1;
			if (is_gateway) {
				ESP_LOGD("$BANG", "Uploading local bang to cloud...");
				bang_upload(chip_id, now_usec);
			} else {
				ESP_LOGD("$BANG", "Transmitting bang to gateway...");
				LoRa.idle();
				lora_write((struct lora_packet) { LORA_BANG_HEADER, chip_id, now_usec });
				LoRa.receive(sizeof (struct lora_packet));
			}
			xSemaphoreGive(lock);
		}
	}
}

// Occasionally broadcast ping to gateway or query cloud for updates
static void ping_task(void *arg)
{
	ESP_LOGI("$PING", "Starting ping task...");

	while (true) {
		delay(random(PING_MIN_MSEC, PING_MAX_MSEC));
		xSemaphoreTake(lock, portMAX_DELAY);
		ESP_LOGI("$PING", "Broadcasting ping...");
		if (is_gateway) {
			if (!ota_version) {
				ESP_LOGI("$PING", "Flagging current version for OTA consideration...");
				ota_version = version;
			}
		} else {
			ESP_LOGD("$PING", "Transmitting version ping to gateway...");
			LoRa.idle();
			lora_write((struct lora_packet) { LORA_PING_HEADER, chip_id, version });
			LoRa.receive(sizeof (struct lora_packet));
		}
		xSemaphoreGive(lock);
	}
}

// Fetch OTA chunks from cloud for ota_version and push to ota_queue
static void gateway_get_task(void *arg)
{
	ESP_LOGI("$GET", "Starting get task...");

	while (true) {
		ESP_LOGD("$GET", "Waiting for version for OTA consideration...");
		while (!ota_version)
			delay(1 * MSECS_TO_SEC);

		char path[256];
		esp_err_t err;
		esp_http_client_config_t http_config = {};
		esp_http_client_handle_t http_handle;
		int64_t read_len[2] = { 0, 0 };
		int status_code;

		ESP_LOGD("$GET", "Querying OTA for version %llX...", ota_version);
		snprintf(path, sizeof path, "/update?version=sensor.%llX", ota_version);
		http_config.host = CLOUD_HOST;
		http_config.port = CLOUD_PORT;
		http_config.path = path;
		if (!(http_handle = esp_http_client_init(&http_config))) {
			ESP_LOGE("$GET", "Unable to create HTTP client");
			goto end0;
		}
		if ((err = esp_http_client_open(http_handle, 0)) != ESP_OK) {
			ESP_LOGE("$GET", "Unable to open HTTP connection: %s (err %d)", esp_err_to_name(err), err);
			goto end1;
		}
		if ((read_len[0] = esp_http_client_fetch_headers(http_handle)) < 0) {
			ESP_LOGE("$GET", "Unable to read HTTP headers");
			goto end1;
		}
		if ((status_code = esp_http_client_get_status_code(http_handle)) != 200) {
			ESP_LOGE("$GET", "Received non-200 status code: %d", status_code);
			goto end1;
		}
		if (!read_len[0]) {
			ESP_LOGD("$GET", "No OTA for version %llX received", ota_version);
			goto end1;
		}

		ESP_LOGI("$GET", "Streaming OTA for version %llX (%lld bytes)...", ota_version, read_len[0]);
		xQueueSend(ota_queue, read_len, portMAX_DELAY);
		while (read_len[1] < read_len[0]) {
			int len, ota_chunks_len = 0;
			lora_payload_t ota_chunks[32] = {};
			if ((len = esp_http_client_read(http_handle, (char *) *ota_chunks, 32 * sizeof (lora_payload_t))) < 0) {
				ESP_LOGE("$GET", "Unable to read HTTP data");
				goto end1;
			}
			read_len[1] += len;
			ota_chunks_len = (len + sizeof (lora_payload_t) - 1) / sizeof (lora_payload_t);
			ESP_LOGD("$GET", "OTA bytes: %d (%lld/%lld)", len, read_len[1], read_len[0]);
			for (int i = 0; i < ota_chunks_len; i++)
				xQueueSend(ota_queue, ota_chunks[i], portMAX_DELAY);
		}
		ESP_LOGI("$GET", "Downloading of OTA for version %llX complete.", ota_version);

end1:
		if ((err = esp_http_client_cleanup(http_handle)) != ESP_OK)
			ESP_LOGE("$GET", "Unable to gracefully cleanup HTTP client: %s (err %d)", esp_err_to_name(err), err);
end0:
		while (uxQueueMessagesWaiting(ota_queue))
			delay(1 * MSECS_TO_SEC);

		xSemaphoreTake(lock, portMAX_DELAY);
		ESP_LOGI("$GET", "Finished streaming OTA for version %llX.", ota_version);
		ota_version = 0;
		xSemaphoreGive(lock);
	}
}

// Occasionally query NTP public pool for current time
static void gateway_ntp_task(void *arg)
{
	ESP_LOGI("$NTP", "Starting NTP task...");

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

		ESP_LOGD("$NTP", "Sending NTP request packet...");

		if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) < 0) {
			ESP_LOGE("$NTP", "Unable to create new socket: %s", strerror(errno));
			goto end0;
		}
		timeout.tv_sec = NTP_TIMEOUT_SEC;
		if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout) < 0) {
			ESP_LOGE("$NTP", "Unable to configure socket: %s", strerror(errno));
			goto end1;
		}

		if (!(host = gethostbyname(NTP_HOST)) || !host->h_addr) {
			ESP_LOGE("$NTP", "Unable to get IP address of " NTP_HOST);
			goto end1;
		}
		dst.sin_family = AF_INET;
		dst.sin_addr = *((struct in_addr *) host->h_addr);
		dst.sin_port = htons(NTP_PORT);
		bzero(&dst.sin_zero, 8);

		packet.li_vn_mode = 0x1b;
		esp_tx_usec = esp_timer_get_time();
		if (sendto(sock, &packet, sizeof packet, 0, (struct sockaddr *) &dst, sizeof dst) < 0) {
			ESP_LOGE("$NTP", "Unable to send request packet: %s", strerror(errno));
			goto end1;
		}
		if (recvfrom(sock, &packet, sizeof packet, 0, (struct sockaddr *) &src, &src_size) < 0) {
			ESP_LOGE("$NTP", "Unable to receive response packet: %s", strerror(errno));
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

		ESP_LOGI("$NTP", "Received NTP response packet from %s - Time difference: %f secs", inet_ntoa(dst.sin_addr), time_diff_sec);

		// The next NTP request should be scheduled sooner if there was a large change in time
		delay_msec = MSECS_TO_SEC - constrain(abs(time_diff_sec / NTP_MAX_DIFF_SEC), 0, 1) * MSECS_TO_SEC;
		delay_msec = NTP_POLL_MIN_SEC * MSECS_TO_SEC + (NTP_POLL_MAX_SEC - NTP_POLL_MIN_SEC) * delay_msec;

end1:
		if (close(sock) < 0)
			ESP_LOGE("$NTP", "Unable to close socket: %s", strerror(errno));
end0:
		ESP_LOGD("$NTP", "Scheduled next NTP query: %f secs", delay_msec / (double) MSECS_TO_SEC);
		delay(delay_msec);
	}
}

// Regularly broadcast current time and queued OTAs over LoRa
static void gateway_sync_task(void *arg)
{
	ESP_LOGI("$SYNC", "Starting sync task...");

	while (true) {
		xSemaphoreTake(lock, portMAX_DELAY);
		ESP_LOGI("$SYNC", "Broadcasting sync...");

		int ota_chunks_len = 0;
		lora_payload_t ota_chunks[LORA_BURST_CAP] = {};
		for (; ota_chunks_len < LORA_BURST_CAP && xQueueReceive(ota_queue, ota_chunks + ota_chunks_len, 0); ota_chunks_len++);

		LoRa.idle();
		ESP_LOGD("$SYNC", "Transmitting sync packet...");
		lora_write((struct lora_packet) { LORA_SYNC_HEADER, ntp_time_usec + esp_timer_get_time() - esp_time_usec, ota_version });
		if (ota_chunks_len && ota_version) {
			ESP_LOGD("$SYNC", "Piggybacking %d OTA packets for version %llX...", ota_chunks_len, ota_version);
			for (int i = 0; i < ota_chunks_len; i++) {
				ESP_LOGD("$SYNC", "Sending OTA packet: %llX:%llX", ota_chunks[i][0], ota_chunks[i][1]);
				lora_write((struct lora_packet) { LORA_OTAS_HEADER, { ota_chunks[i][0], ota_chunks[i][1] } });
			}
		}
		LoRa.receive(sizeof (struct lora_packet));

		if (ota_chunks_len && ota_version == version) {
			ESP_LOGD("$SYNC", "Applying applicable OTA chunks to self...");
			ota_write(ota_chunks, ota_chunks_len);
		}

		xSemaphoreGive(lock);
		delay(SYNC_MSEC);
	}
}

// Handle LoRa packets received by gateway
static void gateway_lora_task(void *arg)
{
	while (true) {
		struct lora_msg msg;
		xQueueReceive(msg_queue, &msg, portMAX_DELAY);

		xSemaphoreTake(lock, portMAX_DELAY);
		if (msg.packet.header == LORA_PING_HEADER) {
			ESP_LOGD("$LORA", "Received ping packet: id=%llu ver=%llX", msg.packet.payload[0], msg.packet.payload[1]);
			if (!ota_version) {
				ESP_LOGI("$LORA", "Flagging ping version for OTA consideration...");
				ota_version = msg.packet.payload[1];
			}
		} else if (msg.packet.header == LORA_BANG_HEADER) {
			ESP_LOGD("$LORA", "Received bang packet: id=%llu usec=%llu", msg.packet.payload[0], msg.packet.payload[1]);
			bang_upload(msg.packet.payload[0], msg.packet.payload[1]);
		}
		xSemaphoreGive(lock);
	}
}

// Handle LoRa packets received by remote
static void remote_lora_task(void *arg)
{
	while (true) {
		if (!uxQueueMessagesWaiting(msg_queue)) { // poor man's async processing
			while (!uxQueueMessagesWaiting(msg_queue))
				delay(1 * MSECS_TO_SEC);
			delay(1 * MSECS_TO_SEC);
		}

		struct lora_msg msg;
		xQueueReceive(msg_queue, &msg, portMAX_DELAY);

		xSemaphoreTake(lock, portMAX_DELAY);
		if (msg.packet.header == LORA_SYNC_HEADER) {
			ESP_LOGD("$LORA", "Received sync packet: usec=%llu ota=%llX", msg.packet.payload[0], msg.packet.payload[1]);
			ntp_time_usec = msg.packet.payload[0];
			esp_time_usec = msg.usec;
			ota_version = msg.packet.payload[1];
		} else if (msg.packet.header == LORA_OTAS_HEADER) {
			ESP_LOGD("$LORA", "Received OTA packet: %llX:%llX", msg.packet.payload[0], msg.packet.payload[1]);
			if (ota_version == version) {
				lora_payload_t ota_chunks[1] = { { msg.packet.payload[0], msg.packet.payload[1] } };
				ota_write(ota_chunks, 1);
			}
		}
		xSemaphoreGive(lock);
	}
}

// Entry point
extern "C" void app_main()
{
	ESP_LOGI("$MAIN", "Initialising application...");
	initArduino();
	Serial.begin(115200);
	log_handler = esp_log_set_vprintf(log_handler_wrapper);
	esp_log_level_set("$MAIN", ESP_LOG_DEBUG);
	esp_log_level_set("$OTA", ESP_LOG_DEBUG);
	esp_log_level_set("$OLED", ESP_LOG_DEBUG);
	esp_log_level_set("$PING", ESP_LOG_DEBUG);
	esp_log_level_set("$GET", ESP_LOG_DEBUG);
	esp_log_level_set("$NTP", ESP_LOG_DEBUG);
	esp_log_level_set("$SYNC", ESP_LOG_DEBUG);
	esp_log_level_set("$LORA", ESP_LOG_DEBUG);
	lock = xSemaphoreCreateMutex(),
	msg_queue = xQueueCreate(LORA_BURST_CAP * 2, sizeof (struct lora_msg));
	ota_queue = xQueueCreate(LORA_BURST_CAP * 2, sizeof (lora_payload_t));
	xTaskCreate(oled_task, "oled", TASK_STACK_SIZE, NULL, TASK_PRIORITY, NULL);

	ESP_LOGW("$MAIN", "Attempting to connecting to local WiFi AP...");
	if (chip_id == 0xfc8b66f23a08) { // TODO(clean): hardcoded
		WiFi.begin(WIFI_SSID, WIFI_PSWD);
		for (int i = 0; WiFi.status() != WL_CONNECTED && i < 30; i++)
			delay(1 * MSECS_TO_SEC);
		if (WiFi.status() != WL_CONNECTED) {
			ESP_LOGE("$MAIN", "Failed to connect to WiFi, rebooting...");
			esp_restart();
		}
	}

	xSemaphoreTake(lock, portMAX_DELAY);
	is_connecting = false;
	is_gateway = (WiFi.status() == WL_CONNECTED);
	SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
	LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
	LoRa.begin(LORA_BAND);
	LoRa.enableCrc();
	LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
	LoRa.setCodingRate4(LORA_CODING_RATE_4);
	LoRa.onReceive(lora_read);
	LoRa.receive(sizeof (struct lora_packet));
	if (is_gateway) {
		ESP_LOGW("$MAIN", "WiFi connection established, performing as gateway sensor device...");
		xTaskCreate(gateway_ntp_task, "ntp", TASK_STACK_SIZE, NULL, TASK_PRIORITY, NULL);
		xTaskCreate(gateway_get_task, "get", TASK_STACK_SIZE, NULL, TASK_PRIORITY, NULL);
		xTaskCreate(gateway_lora_task, "lora", TASK_STACK_SIZE, NULL, TASK_PRIORITY, NULL);
	} else {
		ESP_LOGW("$MAIN", "WiFi not connected, performing as remote sensor device...");
		xTaskCreate(remote_lora_task, "lora", TASK_STACK_SIZE, NULL, TASK_PRIORITY, NULL);
		WiFi.mode(WIFI_OFF);
	}
	xSemaphoreGive(lock);

	ESP_LOGW("$MAIN", "Waiting for first time sync before starting bang detection...");
	while (!ntp_time_usec)
		delay(1 * MSECS_TO_SEC);

	ESP_LOGW("$MAIN", "Time synchronised, starting bang detection...");
	xTaskCreate(bang_task, "bang", TASK_STACK_SIZE, NULL, TASK_PRIORITY, NULL);
	xTaskCreate(ping_task, "ping", TASK_STACK_SIZE, NULL, TASK_PRIORITY, NULL);
	if (is_gateway) {
		xTaskCreate(gateway_sync_task, "sync", TASK_STACK_SIZE, NULL, TASK_PRIORITY, NULL);
	}
}
