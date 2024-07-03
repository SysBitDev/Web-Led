#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "wifi.h"
#include "http.h"
#include "led.h"
#include "motion.h"

/// @brief Main application entry point
void app_main(void) {
    // Initialize NVS (Non-Volatile Storage)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());  // Erase if no free pages or new version found
        ret = nvs_flash_init();  // Reinitialize NVS
    }
    ESP_ERROR_CHECK(ret);  // Check for errors during NVS initialization

    // Initialize Wi-Fi, LED strip, motion sensor, and start the web server
    wifi_init();
    led_strip_init();
    motion_sensor_init();
    start_webserver();
}
