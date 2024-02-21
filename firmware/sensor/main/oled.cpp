#include "oled.hpp"
#include "ntp.hpp"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_log.h>
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
#define DISPLAY_TIME_MSEC 5000

static TimerHandle_t draw_timer;
static TimerHandle_t hide_timer;
static Adafruit_SSD1306 *oled = NULL;

static void on_draw_timeout(TimerHandle_t timer)
{
	oled->clearDisplay();
	oled->setCursor(0, 0);
	if (WiFi.status() != WL_CONNECTED) {
		oled->printf("Connecting to WiFi...");
	} else if (false) { // TODO
		oled->printf("Updating firmware....");
	} else {
		int64_t time_usec = bnet_ntp_time_usec();
		double time_secs = ((double) time_usec) / USECS_TO_SEC;
		double frac_secs = time_secs - ((int64_t) time_secs);
		oled->printf("Seconds since epoch:\n%f\n\n", time_secs);
		oled->fillRect(0, oled->height() - 3, frac_secs * oled->width(), oled->height(), SSD1306_WHITE);
	}
	oled->display();
}

static void on_hide_timeout(TimerHandle_t timer)
{
	ESP_LOGI(TAG, "Disabling OLED display...");
	xTimerStop(draw_timer, 0);
	digitalWrite(RST, LOW);
	delete oled;
	oled = NULL;
	Wire.end();
}

void bnet_oled_show()
{
	ESP_LOGI(TAG, "Enabling OLED display...");

	if (!draw_timer)
		draw_timer = xTimerCreate("oled_draw_timer", DRAW_TIME_MSEC / portTICK_PERIOD_MS, true, NULL, on_draw_timeout);

	if (!hide_timer)
		hide_timer = xTimerCreate("oled_hide_timer", DISPLAY_TIME_MSEC / portTICK_PERIOD_MS, false, NULL, on_hide_timeout);

	if (!oled) {
		Wire.begin(SDA, SCL);
		oled = new Adafruit_SSD1306(WIDTH, HEIGHT, &Wire, RST);
		oled->begin(SSD1306_SWITCHCAPVCC, ADDR);
		oled->setTextColor(SSD1306_WHITE);
		xTimerStart(draw_timer, 0);
	}

	xTimerReset(hide_timer, 0);
}
