#include "sdkconfig.h"

#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "nvs_flash.h"

#include <stdio.h>

#include "config.h"
#include "task_button.h"
#include "task_led.h"
#include "task_state.h"
#include "wifi.h"

#define TAG "MAIN"

static TaskHandle_t readButtonTaskHandle = NULL;
static TaskHandle_t blinkTaskHandle = NULL;
static TaskHandle_t readPillStateTaskHandle = NULL;

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init();
    ESP_LOGI(TAG, "connecting to wifi...");
    while (!wifi_connected(1000)) {
    }
    ESP_LOGI(TAG, "connected to wifi");

    // configure the GPIO pins.
    gpio_reset_pin(BLINK_PIN);
    gpio_reset_pin(INPUT_PIN);
    gpio_set_direction(BLINK_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(INPUT_PIN, GPIO_MODE_INPUT);
    gpio_pullup_en(INPUT_PIN);

    // initialize the state task
    init_state_task();

    // create tasks
    xTaskCreate(read_button_task, "readButtonTask", 4096, NULL, 10, &readButtonTaskHandle);
    xTaskCreate(blink_task, "blinkTask", 4096, NULL, 10, &blinkTaskHandle);
    xTaskCreate(read_pill_state_task, "readPillStateTask", 4096, NULL, 10, &readPillStateTaskHandle);
}
