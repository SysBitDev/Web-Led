// #include "motion.h"
// #include "freertos/FreeRTOS.h"
// #include "freertos/queue.h"
// #include "freertos/task.h"
// #include "esp_log.h"
// #include "driver/gpio.h"
// #include "led.h"
// #include "sdkconfig.h"

// static const char *TAG = "motion_sensor";
// static QueueHandle_t motion_evt_queue = NULL;
// static bool effect_running = false;
// static TimerHandle_t motion_timer;

// #define MOTION_SENSOR_GPIO CONFIG_MOTION_SENSOR_GPIO
// #define MOTION_EFFECT_DURATION_MS 15000
// #define TIMER_EXPIRED 0xFFFFFFFF

// void IRAM_ATTR motion_sensor_isr_handler(void* arg) {
//     uint32_t gpio_num = (uint32_t) arg;
//     xQueueSendFromISR(motion_evt_queue, &gpio_num, NULL);
// }

// static void motion_timer_callback(TimerHandle_t xTimer) {
//     uint32_t timer_expired = TIMER_EXPIRED;
//     xQueueSend(motion_evt_queue, &timer_expired, 0);
// }

// static void motion_sensor_task(void* arg) {
//     uint32_t io_num;
//     for(;;) {
//         if(xQueueReceive(motion_evt_queue, &io_num, portMAX_DELAY)) {
//             if (io_num == TIMER_EXPIRED) {
//                 ESP_LOGI(TAG, "Motion timer expired");
//                 led_strip_stairs_off();
//                 effect_running = false;
//             } else {
//                 ESP_LOGI(TAG, "Motion detected on GPIO %" PRIu32, io_num);
//                 if (!effect_running) {
//                     led_strip_stairs_on();
//                     xTimerStart(motion_timer, 0);
//                     effect_running = true;
//                 } else {
//                     xTimerReset(motion_timer, 0);
//                 }
//             }
//         }
//     }
// }

// void motion_sensor_init(void) {
//     motion_evt_queue = xQueueCreate(10, sizeof(uint32_t));
//     gpio_config_t io_conf = {
//         .intr_type = GPIO_INTR_POSEDGE,
//         .mode = GPIO_MODE_INPUT,
//         .pin_bit_mask = (1ULL << MOTION_SENSOR_GPIO),
//         .pull_down_en = 0,
//         .pull_up_en = 1,
//     };
//     gpio_config(&io_conf);
//     gpio_install_isr_service(0);
//     gpio_isr_handler_add(MOTION_SENSOR_GPIO, motion_sensor_isr_handler, (void*) MOTION_SENSOR_GPIO);
//     motion_timer = xTimerCreate("motion_timer", pdMS_TO_TICKS(MOTION_EFFECT_DURATION_MS), pdFALSE, (void*) 0, motion_timer_callback);
//     xTaskCreate(motion_sensor_task, "motion_sensor_task", 2048, NULL, 10, NULL);
// }
