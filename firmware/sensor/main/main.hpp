#ifndef BNET_MAIN_HPP
#define BNET_MAIN_HPP

#include <cstdint>
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/timers.h>

#define WIFI_SSID "msungie"
#define WIFI_PSWD "hamandcheese"
#define WIFI_ATTEMPT_SEC (10)

// NOTE: Run `python3 -m http.server` in /firmware/sensor directory with OTA_HOST set to your IP address
#define OTA_HOST "192.168.199.134"
#define OTA_PORT (8000)
#define OTA_PATH "/build/sensor.bin"

#define MICROPHONE_PIN (13)

#define MSECS_TO_SEC (1000ll)
#define USECS_TO_SEC (MSECS_TO_SEC * 1000ll)

// Gateway->Remote packets
//   Sync: time[8], ota_version[8]
//   OTA: data[16]
// Remote->Gateway packets
//   Ping: 0:board_id[7], version[8]
//   Bang: 1:board_id[7], time[8]
typedef uint16_t bnet_header_t[1];
typedef uint64_t bnet_packet_t[2];

static const bnet_header_t bnet_header = { 0xB4E7 };

struct bnet_state {
	SemaphoreHandle_t lock; // Mutex for non-queue members
	SemaphoreHandle_t lora; // Mutex for LoRa tx
	// Inter-task communication queues
	QueueHandle_t bang_queue; // To be transmitted (over LoRa or WiFi) bang timestamps
	QueueHandle_t ping_queue; // To be transmitted (over LoRa or WiFi) board announcements (inc. version info)
	QueueHandle_t lota_queue; // To be transmitted (over LoRa) delta OTA chunks
	QueueHandle_t ota_queue; // To be applied delta OTA chunks
	// Synchronised time, represented as the usecs since NTP epoch
	uint64_t ntp_time_usec; // Time since NTP epoch
	uint64_t esp_time_usec; // Time since ESP boot
	// Global variables
	bool is_connecting; // Are we still trying to connect to WiFi AP
	bool is_gateway; // Is this board a gateway (i.e. connected to WiFi and being a time lord)
	uint64_t version; // Current firmware version
	uint64_t lota_version; // Target version for delta OTA chunks currently being received
	// Additional info for OLED display
	uint16_t ota_progress; // How many delta OTA chunks have been applied since beginning a new OTA update
	uint16_t bangs_detected; // How many bangs has the microphone detected since boot
};

void bnet_oled(void *raw_state); // on button -> show oled display
void bnet_bang(void *raw_state); // wait for time sync -> on mic -> push to bang queue
void bnet_ping(void *raw_state); // regularly -> push to ping queue
void bnet_ota(void *raw_state); // pull from ota queue -> apply delta ota

void bnet_time_gateway(void *raw_state); // regularly -> wifi -> sets time
void bnet_lora_gateway(void *raw_state); // on lora rx -> push to bang or ping queue
void bnet_bang_gateway(void *raw_state); // pull from bang queue -> wifi
void bnet_ping_gateway(void *raw_state); // pull from ping queue -> wifi -> push to lota queue
void bnet_sync_gateway(void *raw_state); // regularly -> try pull from lota queue -> lora tx -> maybe push to ota queue

void bnet_lora_remote(void *raw_state); // on lora rx -> sets time -> maybe push to ota queue
void bnet_bang_remote(void *raw_state); // pull from bang queue -> lora tx
void bnet_ping_remote(void *raw_state); // pull from ping queue -> lora tx

#endif // BNET_MAIN_HPP
