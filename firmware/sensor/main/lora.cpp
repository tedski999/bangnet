#include "main.hpp"
#include <LoRa.h>
#include <esp_log.h>

#define TAG "ESP32resso LORA"

#define SCK (5)
#define MISO (19)
#define MOSI (27)
#define SS (18)
#define RST (14)
#define DIO0 (26)
#define BAND (868e6)

static QueueHandle_t packet_queue;

static void on_receive(int len)
{
	bnet_header_t header;
	bnet_packet_t packet;
	LoRa.readBytes((uint8_t *) header, sizeof header);
	LoRa.readBytes((uint8_t *) packet, sizeof packet);
	if (*header != *bnet_header)
		return;
	xQueueSendToBack(packet_queue, packet, portMAX_DELAY);
}

static void setup_receiver(void)
{
	ESP_LOGI(TAG, "Setting up LoRa receiver...");
	packet_queue = xQueueCreate(64, sizeof (bnet_packet_t));
	SPI.begin(SCK, MISO, MOSI, SS);
	LoRa.setPins(SS, RST, DIO0);
	LoRa.begin(BAND); // TODO: lora params
	LoRa.onReceive(on_receive);
	LoRa.receive(sizeof (bnet_header_t) + sizeof (bnet_packet_t));
}

void bnet_lora_gateway(void *raw_state)
{
	ESP_LOGW(TAG, "Starting gateway LoRa task...");
	bnet_state *state = (struct bnet_state *) raw_state;
	setup_receiver();

	while (true) {
		ESP_LOGI(TAG, "Waiting for packet item...");
		bnet_packet_t packet;
		xQueueReceive(packet_queue, packet, portMAX_DELAY);
		ESP_LOGI(TAG, "Got packet item.");

		if (packet[0] & 1) {
			ESP_LOGE(TAG, "Handling remote bang packet...");
			ESP_LOGI(TAG, "Sending bang item...");
			xQueueSendToBack(state->bang_queue, packet, portMAX_DELAY);
			ESP_LOGI(TAG, "Sent bang item.");
			ESP_LOGI(TAG, "Handled remote bang packet.");
		} else {
			ESP_LOGE(TAG, "Handling remote ping packet...");
			ESP_LOGI(TAG, "Sending ping item...");
			xQueueSendToBack(state->ping_queue, packet, portMAX_DELAY);
			ESP_LOGI(TAG, "Sent ping item.");
			ESP_LOGI(TAG, "Handled remote ping packet.");
		}

		// ESP_LOGI(TAG, "RSSI: %s", LoRa.packetRssi());
		// ESP_LOGI(TAG, "Snr: %s", LoRa.packetSnr());
	}
}

void bnet_lora_remote(void *raw_state)
{
	ESP_LOGW(TAG, "Starting remote LoRa task...");
	bnet_state *state = (struct bnet_state *) raw_state;
	setup_receiver();

	while (true) {
		ESP_LOGI(TAG, "Waiting for sync item...");
		bnet_packet_t sync;
		xQueueReceive(packet_queue, sync, portMAX_DELAY);
		ESP_LOGI(TAG, "Got sync item.");

		ESP_LOGE(TAG, "Handling gateway sync packet...");
		uint64_t esp_time_usec = esp_timer_get_time();
		xSemaphoreTake(state->lock, portMAX_DELAY);
		state->ntp_time_usec = sync[0];
		state->esp_time_usec = esp_time_usec;
		xSemaphoreGive(state->lock);
		ESP_LOGI(TAG, "Handled sync packet.");

		// TODO: if sync[1] matches current version:
		//   recv N more packets
		//   push to ota queue

		// TODO(optimisation): idle until next receive window
	}
}
