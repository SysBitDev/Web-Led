#include "motion.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "led.h"
#include "sdkconfig.h"

static const char *TAG = "motion_sensor";
static QueueHandle_t motion_evt_queue = NULL;
static bool effect_running = false;
static TimerHandle_t motion_timer;

#define MOTION_SENSOR_GPIO CONFIG_MOTION_SENSOR_GPIO
#define MOTION_EFFECT_DURATION_MS 5000 // Duration of the effect in milliseconds

/// @brief ISR handler for motion detected.
void IRAM_ATTR motion_sensor_isr_handler(void* arg) {
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(motion_evt_queue, &gpio_num, NULL);
}

/// @brief Task to handle motion sensor events.
static void motion_sensor_task(void* arg) {
    uint32_t io_num;
    for(;;) {
        if(xQueueReceive(motion_evt_queue, &io_num, portMAX_DELAY)) {
            ESP_LOGI(TAG, "Motion detected on GPIO %" PRIu32, io_num);
            if (!effect_running) {
                effect_running = true;
                led_strip_stairs_effect(); // Start stairs effect when motion is detected
                xTimerStart(motion_timer, 0); // Start the timer to stop the effect
            }
        }
    }
}

/// @brief Timer callback to stop the effect.
static void motion_timer_callback(TimerHandle_t xTimer) {
    effect_running = false;
    led_strip_stop_effect(); // Stop the effect after the timer expires
}

/// @brief Initializes the motion sensor.
void motion_sensor_init(void) {
    motion_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_POSEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << MOTION_SENSOR_GPIO),
        .pull_down_en = 0,
        .pull_up_en = 1,
    };
    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(MOTION_SENSOR_GPIO, motion_sensor_isr_handler, (void*) MOTION_SENSOR_GPIO);
    xTaskCreate(motion_sensor_task, "motion_sensor_task", 2048, NULL, 10, NULL);
    motion_timer = xTimerCreate("motion_timer", pdMS_TO_TICKS(MOTION_EFFECT_DURATION_MS), pdFALSE, (void*) 0, motion_timer_callback);
}
