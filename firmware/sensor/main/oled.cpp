#include "main.hpp"
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

#define TAG "ESP32resso OLED"

#define SHOW_MSEC 30000
#define POLL_MSEC 50

#define SDA 4
#define SCL 15
#define RST 16
#define ADDR 0x3c
#define WIDTH 128
#define HEIGHT 64
#define BUTTON_PIN 0

static void stub_timer(TimerHandle_t timer) {}

void bnet_oled(void *raw_state)
{
	ESP_LOGW(TAG, "Starting OLED display task...");
	struct bnet_state *state = (struct bnet_state *) raw_state;
	Adafruit_SSD1306 *oled = NULL;
	TimerHandle_t timer = xTimerCreate("oled", SHOW_MSEC / portTICK_PERIOD_MS, false, NULL, stub_timer);
	xTimerStart(timer, portMAX_DELAY);
	pinMode(BUTTON_PIN, INPUT_PULLUP);

	while (true) {
		if (digitalRead(BUTTON_PIN) == 0) {
			ESP_LOGI(TAG, "Button down, resetting timer...");
			xTimerReset(timer, portMAX_DELAY);
		}

		if (xTimerIsTimerActive(timer) && !oled) {
			ESP_LOGW(TAG, "Enabling display...");
			Wire.begin(SDA, SCL);
			oled = new Adafruit_SSD1306(WIDTH, HEIGHT, &Wire, RST);
			oled->begin(SSD1306_SWITCHCAPVCC, ADDR);
			oled->setTextColor(SSD1306_WHITE);
			digitalWrite(RST, HIGH);
		} else if (!xTimerIsTimerActive(timer) && oled) {
			ESP_LOGW(TAG, "Disabling display...");
			digitalWrite(RST, LOW);
			delete oled;
			oled = NULL;
			Wire.end();
		}

		if (oled) {
			oled->clearDisplay();
			oled->setCursor(0, 0);
			xSemaphoreTake(state->lock, portMAX_DELAY);

			const esp_app_desc_t *app_desc = esp_app_get_description();
			oled->printf("%llX %.8s\n", ESP.getEfuseMac(), app_desc->version);

			if (state->is_connecting) {
				oled->printf("Trying: " WIFI_SSID "\n");
			} else {
				oled->printf("WiFi: %.16s\n", state->is_gateway ? WIFI_SSID : "none");
				oled->printf("OTA: %u\n", state->ota_progress);
				oled->printf("Bangs: %u\n", state->bangs_detected);
				oled->printf("Sound: %d\n", analogRead(MICROPHONE_PIN));
				oled->printf("=> ");
				if (state->esp_time_usec) {
					uint64_t now = state->ntp_time_usec + esp_timer_get_time() - state->esp_time_usec;
					double time_sec = now / (double) USECS_TO_SEC;
					double frac_sec = time_sec - (int64_t) time_sec;
					oled->printf("%f\n", time_sec);
					oled->fillRect(0, oled->height() - 3, frac_sec * oled->width(), oled->height(), SSD1306_WHITE);
				} else {
					oled->printf("no sync\n");
				}
			}

			xSemaphoreGive(state->lock);
			oled->display();
		}

		delay(POLL_MSEC);
	}
}
