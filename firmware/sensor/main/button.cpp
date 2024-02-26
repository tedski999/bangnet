#include "button.hpp"
#include "oled.hpp"
#include <Arduino.h>
#include <esp_log.h>
#include <freertos/task.h>

#define TAG "ESP32resso Button"

#define TASK_STACK_SIZE (8096)
#define TASK_PRIORITY (5)

#define BUTTON_PIN 0
#define BUTTON_TIME_MSEC 200

#define OLED_TIME_MSEC 10000

// Light up OLED display for 10 seconds every time button is pressed
static void task(void *args)
{
	while (true) {
		if (digitalRead(BUTTON_PIN) == 0)
			bnet_oled_show(OLED_TIME_MSEC);
		delay(BUTTON_TIME_MSEC);
	}
}

// Regularly poll button for if pressed
void bnet_button_start() {
	pinMode(BUTTON_PIN, INPUT_PULLUP);
	if (xTaskCreate(task, "button task", TASK_STACK_SIZE, NULL, TASK_PRIORITY, NULL) != pdPASS)
		ESP_LOGE(TAG, "Unable to create task");
}
