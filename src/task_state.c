
#include "cJSON.h"
#include "config.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"

// poll server every minute
#define SERVER_POLL_INTERVAL_MS (10 * 1000)
#define TAG "state"

static SemaphoreHandle_t minuteSem = NULL;
static int minutesSinceLastPill = 0;

void init_state_task() {
    minuteSem = xSemaphoreCreateMutex();
}

// Event handler for HTTP client.  This receives the data from the server,
// parses the JSON response and updates the minutesSinceLastPill global.
esp_err_t client_event_get_handler(esp_http_client_event_handle_t evt) {
    char buffer[100];
    int value = 0;

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
        ESP_LOGI(TAG, "minutes: %d", value);
        break;

    default:
        break;
    }

    return ESP_OK;
}

void read_pill_state_task(void *arg) {

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

int get_minutes_since_last_pill() {
    int mins;
    xSemaphoreTake(minuteSem, portMAX_DELAY);
    mins = minutesSinceLastPill;
    xSemaphoreGive(minuteSem);
    return mins;
}

void reset_minutes_since_last_pill() {
    xSemaphoreTake(minuteSem, portMAX_DELAY);
    minutesSinceLastPill = 0;
    xSemaphoreGive(minuteSem);
}