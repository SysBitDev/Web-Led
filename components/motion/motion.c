#include "motion.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "led.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "time_sun.h"

static const char *TAG = "motion_sensor";

#define MOTION_SENSOR_GPIO GPIO_NUM_19

static void motion_sensor_isr_handler(void *arg);
static void motion_task(void *arg);
static SemaphoreHandle_t motion_semaphore = NULL;
static TaskHandle_t motion_task_handle = NULL;

static uint32_t motion_delay_ms = 1000;

void motion_init(void) {
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_POSEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << MOTION_SENSOR_GPIO),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE
    };
    gpio_config(&io_conf);

    if (motion_semaphore == NULL) {
        motion_semaphore = xSemaphoreCreateBinary();
    }

    gpio_install_isr_service(0);
    gpio_isr_handler_add(MOTION_SENSOR_GPIO, motion_sensor_isr_handler, NULL);
}

static void IRAM_ATTR motion_sensor_isr_handler(void *arg) {
    gpio_intr_disable(MOTION_SENSOR_GPIO);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(motion_semaphore, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}


void motion_start(void) {
    if (motion_task_handle == NULL) {
        xTaskCreate(motion_task, "motion_task", 4096, NULL, 5, &motion_task_handle);
    }
}

void motion_stop(void) {
    if (motion_task_handle != NULL) {
        vTaskDelete(motion_task_handle);
        motion_task_handle = NULL;
    }
}

static void motion_task(void *arg) {
    while (1) {
        if (xSemaphoreTake(motion_semaphore, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "Motion detected!");

            if (!is_night_time) {
                ESP_LOGI(TAG, "It's daytime. Not turning on LEDs.");
                gpio_intr_enable(MOTION_SENSOR_GPIO);
                continue;
            }

            if (!led_strip_is_effect_running()) {
                led_strip_stairs_effect();
            } else {
                ESP_LOGI(TAG, "The effect is already underway, we are waiting for completion");
            }
            while (led_strip_is_effect_running()) {
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            gpio_intr_enable(MOTION_SENSOR_GPIO);
        }
    }
}


uint32_t motion_get_delay(void) {
    return motion_delay_ms;
}

void motion_set_delay(uint32_t delay_ms) {
    motion_delay_ms = delay_ms;
}
