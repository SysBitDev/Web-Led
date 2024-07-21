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

/**
 * @brief Main application entry point.
 * Initializes system components and services. This function sets up NVS,
 * Wi-Fi, LED functionality, motion sensors, and starts the HTTP server for web interaction.
 */
void app_main(void) {
    // Initialize Non-Volatile Storage (NVS).
    esp_err_t ret = nvs_flash_init();
    // Check if the NVS needs to be erased due to a full flash or new version found,
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // Erase the NVS flash and retry initialization if needed.
        ESP_ERROR_CHECK(nvs_flash_erase());  // Erase the NVS flash.
        ret = nvs_flash_init();  // Re-initialize NVS.
    }
    ESP_ERROR_CHECK(ret);  // Check for any errors during NVS initialization and handle failures.

    // Initialize subsystems for Wi-Fi connectivity. This will set up the ESP32's Wi-Fi capabilities,
    wifi_init();

    // Initialize the LED control functionality. This component controls the behavior of LED strips,
    led_strip_init();

    // Initialize motion sensors. This involves setting up GPIO or other interfaces to detect motion,
    motion_sensor_init();

    // Start the web server to allow web-based control interfaces.
    start_webserver();
}
