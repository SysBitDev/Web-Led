#include "motion.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "led.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "time_sun.h"

static const char *TAG = "motion_sensor";

#define MOTION_SENSOR_GPIO_1 GPIO_NUM_19
#define MOTION_SENSOR_GPIO_2 GPIO_NUM_21

static void motion_sensor_isr_handler(void *arg);
static void motion_task(void *arg);
static QueueHandle_t motion_queue = NULL;
static TaskHandle_t motion_task_handle = NULL;

static uint32_t motion_delay_ms = 1000;

void motion_init(void) {
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_POSEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << MOTION_SENSOR_GPIO_1) | (1ULL << MOTION_SENSOR_GPIO_2),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE
    };
    gpio_config(&io_conf);

    if (motion_queue == NULL) {
        motion_queue = xQueueCreate(10, sizeof(uint32_t));
    }

    gpio_install_isr_service(0);
    gpio_isr_handler_add(MOTION_SENSOR_GPIO_1, motion_sensor_isr_handler, (void *)MOTION_SENSOR_GPIO_1);
    gpio_isr_handler_add(MOTION_SENSOR_GPIO_2, motion_sensor_isr_handler, (void *)MOTION_SENSOR_GPIO_2);
}

static void IRAM_ATTR motion_sensor_isr_handler(void *arg) {
    uint32_t gpio_num = (uint32_t)arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    xQueueSendFromISR(motion_queue, &gpio_num, &xHigherPriorityTaskWoken);

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
    uint32_t gpio_num;
    uint32_t sensor1_time = 0;
    uint32_t sensor2_time = 0;
    const uint32_t activation_window_ms = 500;

    while (1) {
        if (xQueueReceive(motion_queue, &gpio_num, portMAX_DELAY) == pdTRUE) {
            uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

            if (gpio_num == MOTION_SENSOR_GPIO_1) {
                ESP_LOGI(TAG, "Motion detected on Sensor 1!");
                sensor1_time = current_time;
            } else if (gpio_num == MOTION_SENSOR_GPIO_2) {
                ESP_LOGI(TAG, "Motion detected on Sensor 2!");
                sensor2_time = current_time;
            }

            if (!is_night_time && !ignore_sun) {
                ESP_LOGI(TAG, "It's daytime and sun is not ignored. Not turning on LEDs.");
                continue;
            }

            if (led_strip_is_effect_running()) {
                ESP_LOGI(TAG, "Effect is already running, ignoring new triggers.");
                continue;
            }

            if (abs((int32_t)(sensor1_time - sensor2_time)) <= activation_window_ms && sensor1_time > 0 && sensor2_time > 0) {
                ESP_LOGI(TAG, "Both sensors activated nearly simultaneously.");
                led_strip_stairs_effect_both();
                sensor1_time = sensor2_time = 0;
            } else if (sensor1_time > 0 && (current_time - sensor1_time) > activation_window_ms) {
                ESP_LOGI(TAG, "Starting effect from Sensor 1 side.");
                led_strip_stairs_effect_from_start();
                sensor1_time = 0;
            } else if (sensor2_time > 0 && (current_time - sensor2_time) > activation_window_ms) {
                ESP_LOGI(TAG, "Starting effect from Sensor 2 side.");
                led_strip_stairs_effect_from_end();
                sensor2_time = 0;
            }
        }
    }
}

uint32_t motion_get_delay(void) {
    return motion_delay_ms;
}

void motion_set_delay(uint32_t delay_ms) {
    motion_delay_ms = delay_ms;
}
