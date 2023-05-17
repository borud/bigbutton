#include "esp_chip_info.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include <inttypes.h>
#include <stdio.h>

#include "config.h"
#include "wifi.h"

#define WIFI_CONNECTED_BIT BIT0

static const char *TAG = "NET";
static EventGroupHandle_t wifi_event_group;

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    ip_event_got_ip_t *got_ip_event;

    switch (event_id) {
    case WIFI_EVENT_STA_START:
        esp_wifi_connect();
        ESP_LOGI(TAG, "[%s] initial connect to the AP", event_base);
        break;

    case WIFI_EVENT_STA_CONNECTED:
        ESP_LOGI(TAG, "[%s] connected to wifi, starting DHCP", event_base);
        break;

    case WIFI_EVENT_STA_DISCONNECTED:
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        esp_wifi_connect();
        ESP_LOGI(TAG, "[%s] disconnect, retrying connect to AP", event_base);
        break;

    case IP_EVENT_STA_GOT_IP:
        got_ip_event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "[%s] got IP ip=%d.%d.%d.%d", event_base, IP2STR(&got_ip_event->ip_info.ip));

        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        break;

    case WIFI_EVENT_STA_STOP:
        ESP_LOGI(TAG, "[%s] stopped", event_base);
        break;
    }
}

void wifi_init(void) {
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_config));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta =
            {
                .ssid = CONFIG_WIFI_SSID,
                .password = CONFIG_WIFI_PASSWORD,
                .threshold.authmode = WIFI_AUTH_WPA2_PSK,
                .pmf_cfg = {.capable = true, .required = false},
            },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "wifi initialization finished ssid='%s' password='%s'", CONFIG_WIFI_SSID,
             CONFIG_WIFI_PASSWORD);
}

bool wifi_connected(int wait_ms) {
    return (xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE,
                                (wait_ms / portTICK_PERIOD_MS) & WIFI_CONNECTED_BIT));
}
