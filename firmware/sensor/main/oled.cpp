#include "oled.hpp"
#include "ntp.hpp"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_pm.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

#define TAG "ESP32resso OLED"

#define SDA 4
#define SCL 15
#define RST 16
#define ADDR 0x3c
#define WIDTH 128
#define HEIGHT 64

#define DRAW_TIME_MSEC 50

static TimerHandle_t draw_timer;
static TimerHandle_t hide_timer;
static Adafruit_SSD1306 *oled = NULL;

// Draw a new status frame on the OLED display
static void on_draw_timeout(TimerHandle_t timer)
{
	oled->clearDisplay();
	oled->setCursor(0, 0);

	// App version
	const esp_app_desc_t *app_desc = esp_app_get_description();
	oled->printf("V: %.16s\n", app_desc->version);

	// WiFi status
	wl_status_t wifi_status = WiFi.status();
	if (wifi_status == WL_STOPPED)
		oled->printf("W: Disabled\n");
	else if (wifi_status == WL_CONNECTED)
		oled->printf("W: %.16s\n", WiFi.SSID().c_str());
	else
		oled->printf("W: Disconnected\n");

	// Power management status
	esp_pm_config_t pm_config;
	if (esp_pm_get_configuration(&pm_config) != ESP_OK) {
		oled->printf("C: error\n");
	} else if (pm_config.min_freq_mhz == 0 && pm_config.min_freq_mhz == 0) {
		oled->printf("C: limits unset\n");
	} else {
		oled->printf("C: %d-%dMHz\n", pm_config.min_freq_mhz, pm_config.max_freq_mhz);
	}

	// NTP status
	int64_t time_usec = bnet_ntp_time_usec();
	if (time_usec != 0) {
		double time_secs = time_usec / (double) USECS_TO_SEC;
		double frac_secs = time_secs - (int64_t) time_secs;
		oled->printf("T: NTP synchronised\n\n%f", time_secs);
		oled->fillRect(0, oled->height() - 3, frac_secs * oled->width(), oled->height(), SSD1306_WHITE);
	} else {
		oled->printf("T: Clock unsynchronised");
	}

	oled->display();
}

// After set time has elapsed, power off the OLED display
static void on_hide_timeout(TimerHandle_t timer)
{
	ESP_LOGI(TAG, "Disabling OLED display...");
	xTimerStop(draw_timer, portMAX_DELAY);
	digitalWrite(RST, LOW);
	delete oled;
	oled = NULL;
	Wire.end();
}

// Light the OLED display for some time
void bnet_oled_show(int duration_msec)
{
	ESP_LOGI(TAG, "Enabling OLED display...");

	if (!draw_timer)
		draw_timer = xTimerCreate("oled_draw_timer", DRAW_TIME_MSEC / portTICK_PERIOD_MS, true, NULL, on_draw_timeout);

	if (!hide_timer)
		hide_timer = xTimerCreate("oled_hide_timer", duration_msec / portTICK_PERIOD_MS, false, NULL, on_hide_timeout);

	if (!oled) {
		Wire.begin(SDA, SCL);
		oled = new Adafruit_SSD1306(WIDTH, HEIGHT, &Wire, RST);
		oled->begin(SSD1306_SWITCHCAPVCC, ADDR);
		oled->setTextColor(SSD1306_WHITE);
		xTimerStart(draw_timer, portMAX_DELAY);
	}

	xTimerReset(hide_timer, portMAX_DELAY);
}
