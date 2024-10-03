// main.c

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "wifi.h"
#include "http.h"
#include "led.h"
#include "motion.h"
#include "mdns.h"

void app_main(void) {
    // Initialize NVS once
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize WiFi
    wifi_init();

    // Initialize LED strip
    led_strip_init();

    // Load LED parameters from NVS and apply settings
    led_strip_load_parameters();

    // Start LED strip with loaded settings
    led_strip_start();

    // Initialize Motion Sensor
    motion_sensor_init();

    // Initialize mDNS
    mdns_init();
    mdns_hostname_set("smart-stairs");
    mdns_instance_name_set("Smart Stairs");

    // Start Web Server
    start_webserver();
}

