#include "motion.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "led.h"
#include "sdkconfig.h"

static const char *TAG = "motion_sensor"; ///< Log tag for motion sensor related logs
static QueueHandle_t motion_evt_queue = NULL; ///< Queue to handle motion sensor events
static bool effect_running = false; ///< Flag to track if the motion-triggered effect is currently running
static TimerHandle_t motion_timer; ///< Timer for controlling motion-triggered LED effects

#define MOTION_SENSOR_GPIO CONFIG_MOTION_SENSOR_GPIO ///< GPIO number for the motion sensor, configured in sdkconfig
#define MOTION_EFFECT_DURATION_MS 5000 ///< Duration in milliseconds for which the motion-triggered effect runs

/**
 * @brief ISR handler for motion detection.
 * This function is called when a motion is detected (rising edge) on the configured GPIO.
 * @param arg The GPIO number associated with the sensor that detected motion.
 */
void IRAM_ATTR motion_sensor_isr_handler(void* arg) {
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(motion_evt_queue, &gpio_num, NULL); // Send the GPIO number to the motion event queue
}

/**
 * @brief Task to handle motion sensor events.
 * This task waits for GPIO numbers from the motion_evt_queue and starts an LED effect if not already running.
 * @param arg User-defined argument, not used in this function.
 */
static void motion_sensor_task(void* arg) {
    uint32_t io_num;
    for(;;) {
        if(xQueueReceive(motion_evt_queue, &io_num, portMAX_DELAY)) { // Wait indefinitely for motion events
            ESP_LOGI(TAG, "Motion detected on GPIO %" PRIu32, io_num);
            if (!effect_running) {
                effect_running = true;
                led_strip_stairs_effect(); // Activate the stairs effect upon detecting motion
                xTimerStart(motion_timer, 0); // Start a timer to stop the effect after a set duration
            }
        }
    }
}

/**
 * @brief Timer callback function to stop the motion-triggered effect.
 * This function is called when the timer expires and stops any running LED effects.
 * @param xTimer Handle to the timer that expired (not used).
 */
static void motion_timer_callback(TimerHandle_t xTimer) {
    effect_running = false;
    led_strip_stop_effect(); // Stop the LED effect when the timer expires
}

/**
 * @brief Initializes the motion sensor interface.
 * Sets up the GPIO pin for input, configures the interrupt, and initializes the task and timer for handling motion events.
 */
void motion_sensor_init(void) {
    motion_evt_queue = xQueueCreate(10, sizeof(uint32_t)); // Create a queue capable of holding 10 uint32_t values
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_POSEDGE, // Interrupt on positive edge
        .mode = GPIO_MODE_INPUT, // Set as input mode
        .pin_bit_mask = (1ULL << MOTION_SENSOR_GPIO), // Configure the motion sensor GPIO
        .pull_down_en = 0, // Disable pull-down resistor
        .pull_up_en = 1, // Enable pull-up resistor
    };
    gpio_config(&io_conf); // Apply the GPIO configuration
    gpio_install_isr_service(0); // Install GPIO ISR service without any flags
    gpio_isr_handler_add(MOTION_SENSOR_GPIO, motion_sensor_isr_handler, (void*) MOTION_SENSOR_GPIO); // Add ISR handler

    xTaskCreate(motion_sensor_task, "motion_sensor_task", 2048, NULL, 10, NULL); // Create the motion sensor task
    motion_timer = xTimerCreate("motion_timer", pdMS_TO_TICKS(MOTION_EFFECT_DURATION_MS), pdFALSE, (void*) 0, motion_timer_callback); // Create the timer for the motion effect
}
