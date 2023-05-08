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

#include "esp_crt_bundle.h"
#include "esp_http_client.h"

#include <stdio.h>

#include "config.h"
#include "wifi.h"

#include "cJSON.h"

#define BLINK_PIN GPIO_NUM_2
#define INPUT_PIN GPIO_NUM_21

#define TAG "BTN"

// #define MINUTE_THRESHOLD_1 720
#define MINUTE_THRESHOLD_1 600
// #define MINUTE_THRESHOLD_2 840
#define MINUTE_THRESHOLD_2 700

// poll server every minute
#define SERVER_POLL_INTERVAL_MS (10 * 1000)

static int minutesSinceLastPill = 0;

static SemaphoreHandle_t minuteSem = NULL;

static TaskHandle_t readButtonTaskHandle = NULL;
static TaskHandle_t blinkTaskHandle = NULL;
static TaskHandle_t readPillStateTaskHandle = NULL;

// Event handler for HTTP client.  This receives the data from the server,
// parses the JSON response and updates the minutesSinceLastPill global.
esp_err_t client_event_get_handler(esp_http_client_event_handle_t evt) {
    char buffer[100];
    int value;

    switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA:
        strncpy(buffer, evt->data, evt->data_len);
        buffer[evt->data_len >= 100 ? 99 : evt->data_len] = 0;

        cJSON *jsn = cJSON_Parse(buffer);
        value = cJSON_GetObjectItem(jsn, "minutes")->valueint;
        cJSON_Delete(jsn);

        xSemaphoreTake(minuteSem, portMAX_DELAY);
        minutesSinceLastPill = value;
        xSemaphoreGive(minuteSem);
        break;

    default:
        break;
    }
    return ESP_OK;
}

// Task for reading the pill state.  Performs HTTP GET on SERVER_DISPLAY_ENDPOINT
// and indirectly updates the minutesSinceLastPill through the callback
// client_event_get_handler.
void readPillStateTask(void *arg) {
    esp_http_client_config_t config_get = {.url = SERVER_DISPLAY_ENDPOINT,
                                           .method = HTTP_METHOD_GET,
                                           .event_handler = client_event_get_handler,
                                           .auth_type = HTTP_AUTH_TYPE_NONE,
                                           .transport_type = HTTP_TRANSPORT_OVER_SSL,
                                           .crt_bundle_attach = esp_crt_bundle_attach,
                                           .user_agent = "BigButton/1.0"};

    esp_http_client_handle_t client = esp_http_client_init(&config_get);

    while (1) {
        esp_http_client_perform(client);
        vTaskDelay(SERVER_POLL_INTERVAL_MS / portTICK_PERIOD_MS);
    }
    // if er rewrite this to start/stop we might want to esp_http_client_cleanup(client);
}

// Task for reading the button state, debounce and POST an empty message
// to the SERVER_POST_ENDPOINT URL.  We should simplify this a bit and split
// the action in two so we perform the HTTP stuff in its own task.
void readButtonTask(void *arg) {
    uint8_t state = 0;
    uint8_t level = 0;

    esp_http_client_config_t config_post = {.url = SERVER_POST_ENDPOINT,
                                            .method = HTTP_METHOD_POST,
                                            .auth_type = HTTP_AUTH_TYPE_NONE,
                                            .transport_type = HTTP_TRANSPORT_OVER_SSL,
                                            .crt_bundle_attach = esp_crt_bundle_attach,
                                            .user_agent = "BigButton/1.0"};
    esp_http_client_handle_t client = esp_http_client_init(&config_post);

    while (1) {
        vTaskDelay(10 / portTICK_PERIOD_MS);
        level = gpio_get_level(INPUT_PIN);
        if (level != state) {
            vTaskDelay(10 / portTICK_PERIOD_MS);
            level = gpio_get_level(INPUT_PIN);
            if (level != state) {
                // trigger on button release transition
                if ((state == 1) && (level == 0)) {
                    ESP_LOGI(TAG, "registering event with server");
                    esp_err_t err = esp_http_client_perform(client);
                    if (err != ERR_OK) {
                        ESP_LOGE(TAG, "HTTP Post request failed: %s", esp_err_to_name(err));
                        // set the value to something absurd.  We should set this to a negative
                        // value to indicate an error.
                        xSemaphoreTake(minuteSem, portMAX_DELAY);
                        minutesSinceLastPill = 1000;
                        xSemaphoreGive(minuteSem);
                    } else {
                        // don't wait for readPillStateTask to loop around
                        xSemaphoreTake(minuteSem, portMAX_DELAY);
                        minutesSinceLastPill = 0;
                        xSemaphoreGive(minuteSem);
                    }
                }

                state = level;
            }
        }
    }
}

// Task for blinking the LED.  This too should be rewritten to something that is
// a bit more elegant.
void blinkTask(void *arg) {
    int minutes = 0;
    int delay = 0;

    while (1) {
        xSemaphoreTake(minuteSem, portMAX_DELAY);
        minutes = minutesSinceLastPill;
        xSemaphoreGive(minuteSem);

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

void app_main(void) {
    minuteSem = xSemaphoreCreateMutex();

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

    xTaskCreate(readButtonTask, "readButtonTask", 4096, NULL, 10, &readButtonTaskHandle);
    xTaskCreate(blinkTask, "blinkTask", 4096, NULL, 10, &blinkTaskHandle);
    xTaskCreate(readPillStateTask, "readPillStateTask", 4096, NULL, 10, &readPillStateTaskHandle);
}
