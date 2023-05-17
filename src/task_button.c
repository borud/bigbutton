#include "freertos/FreeRTOS.h"

#include "driver/gpio.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"

#include "config.h"
#include "task_state.h"

#define TAG "button-task"

// Task for reading the button state, debounce and POST an empty message
// to the SERVER_POST_ENDPOINT URL.  We should simplify this a bit and split
// the action in two so we perform the HTTP stuff in its own task.
void read_button_task(void *arg) {
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
                    } else {
                        // don't wait for readPillStateTask to loop around
                        reset_minutes_since_last_pill();
                    }
                }

                state = level;
            }
        }
    }
}
