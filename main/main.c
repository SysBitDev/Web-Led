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
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init();

    led_strip_init();

    led_strip_load_parameters();

    led_strip_start();

    motion_init();
    motion_start();

    mdns_init();
    mdns_hostname_set("smart-stairs");
    mdns_instance_name_set("Smart Stairs");

    start_webserver();
}

