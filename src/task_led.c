#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/task.h"

#include "config.h"
#include "task_led.h"
#include "task_state.h"

#define TAG "led-task"

#define THRESHOLD_NOW (12 * 60)
#define THRESHOLD_SOON THRESHOLD_NOW - 15
#define THRESHOLD_60MIN (THRESHOLD_NOW + 60)

ledc_timer_config_t ledc_timer = {.speed_mode = LEDC_LOW_SPEED_MODE,
                                  .timer_num = LEDC_TIMER_0,
                                  .duty_resolution = LEDC_TIMER_13_BIT,
                                  .freq_hz = 5000,
                                  .clk_cfg = LEDC_AUTO_CLK};

ledc_channel_config_t ledc_channel = {.speed_mode = LEDC_LOW_SPEED_MODE,
                                      .channel = LEDC_CHANNEL_0,
                                      .timer_sel = LEDC_TIMER_0,
                                      .intr_type = LEDC_INTR_DISABLE,
                                      .gpio_num = BLINK_PIN,
                                      .duty = 0, // Set duty to 0%
                                      .hpoint = 0};

static SemaphoreHandle_t counting_sem;

static IRAM_ATTR bool cb_ledc_fade_end_event(const ledc_cb_param_t *param, void *user_arg) {
    portBASE_TYPE taskAwoken = pdFALSE;

    if (param->event == LEDC_FADE_END_EVT) {
        SemaphoreHandle_t counting_sem = (SemaphoreHandle_t)user_arg;
        xSemaphoreGiveFromISR(counting_sem, &taskAwoken);
    }

    return (taskAwoken == pdTRUE);
}

void dim() {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 3000);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    vTaskDelay(100 / portTICK_PERIOD_MS);
}

void breathe() {
    ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 8000, 2000);
    ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_NO_WAIT);
    xSemaphoreTake(counting_sem, portMAX_DELAY);

    vTaskDelay(500 / portTICK_PERIOD_MS);

    ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0, 1000);
    ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_NO_WAIT);
    xSemaphoreTake(counting_sem, portMAX_DELAY);
}

void blink() {
    // ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 8000, 100);
    ledc_set_fade_with_step(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 8190, 100, 1);
    ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_NO_WAIT);
    xSemaphoreTake(counting_sem, portMAX_DELAY);

    vTaskDelay(100 / portTICK_PERIOD_MS);

    ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0, 500);
    ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_NO_WAIT);
    xSemaphoreTake(counting_sem, portMAX_DELAY);
}

void fast_blink() {
    // ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 8000, 100);
    ledc_set_fade_with_step(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 8190, 200, 1);
    ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_NO_WAIT);
    xSemaphoreTake(counting_sem, portMAX_DELAY);

    vTaskDelay(100 / portTICK_PERIOD_MS);

    ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0, 500);
    ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_NO_WAIT);
    xSemaphoreTake(counting_sem, portMAX_DELAY);
}

// Task for blinking the LED.  This too should be rewritten to something that is
// a bit more elegant.
void blink_task(void *arg) {
    counting_sem = xSemaphoreCreateCounting(1, 0);

    // Configure the LEDC timer
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    ESP_ERROR_CHECK(ledc_fade_func_install(0));
    ledc_cbs_t callbacks = {.fade_cb = cb_ledc_fade_end_event};

    ledc_cb_register(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, &callbacks, (void *)counting_sem);

    while (1) {
        int min = get_minutes_since_last_pill();
        // before soon
        if (min < THRESHOLD_SOON) {
            dim();
            vTaskDelay(100 / portTICK_PERIOD_MS);
            continue;
        }

        // soon
        if (min < THRESHOLD_NOW) {
            breathe();
            vTaskDelay(500 / portTICK_PERIOD_MS);
            continue;
        }

        // between now and 60 min
        if (min < THRESHOLD_60MIN) {
            blink();
            vTaskDelay(500 / portTICK_PERIOD_MS);
            continue;
        }

        // greater than 60 min
        fast_blink();
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
