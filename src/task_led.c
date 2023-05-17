#include "freertos/FreeRTOS.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/task.h"

#include "config.h"
#include "task_led.h"
#include "task_state.h"

#define TAG "led-task"
#define MINUTE_THRESHOLD_1 600
#define MINUTE_THRESHOLD_2 700

// Task for blinking the LED.  This too should be rewritten to something that is
// a bit more elegant.
void blink_task(void *arg) {
    int minutes = 0;
    int delay = 0;

    while (1) {
        minutes = get_minutes_since_last_pill();

        ESP_LOGI(TAG, "blinkTask minutes: %d", minutes);

        if (minutes > MINUTE_THRESHOLD_2) {
            delay = 100;
        } else if (minutes > MINUTE_THRESHOLD_1) {
            delay = 500;
        } else {
            gpio_set_level(BLINK_PIN, 1);
            vTaskDelay(500 / portTICK_PERIOD_MS);
            continue;
        }

        ESP_LOGI(TAG, " - delay %d", delay);

        gpio_set_level(BLINK_PIN, 1);
        vTaskDelay(delay / portTICK_PERIOD_MS);
        gpio_set_level(BLINK_PIN, 0);
        vTaskDelay(delay / portTICK_PERIOD_MS);
    }
}
