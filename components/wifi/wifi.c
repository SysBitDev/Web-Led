#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_smartconfig.h"
#include "nvs.h"
#include "esp_system.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

#include "wifi.h"

const char *TAG = "wifi";
EventGroupHandle_t s_wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;
const int WIFI_FAIL_BIT = BIT1;
const int ESPTOUCH_DONE_BIT = BIT2;

#define BUTTON_GPIO CONFIG_BUTTON_GPIO
#define BUTTON_PRESS_TIMEOUT_MS 1000

int s_retry_num = 0;
#ifndef CONFIG_MAX_RETRY
#define CONFIG_MAX_RETRY 5
#endif

const int MAX_RETRY = CONFIG_MAX_RETRY;

static volatile bool esptouch_done = false;
static volatile bool reset_wifi_config = false;

static void smartconfig_task(void *parm);

void erase_wifi_config(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        nvs_erase_key(nvs_handle, "wifi_ssid");
        nvs_erase_key(nvs_handle, "wifi_pass");
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        ESP_LOGI(TAG, "WiFi configuration erased");
    } else {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
    }
}

void wifi_wait_connected(void) {
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);
}

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        if (!reset_wifi_config) {
            wifi_config_t wifi_config = {0};
            nvs_handle_t nvs_handle;
            esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);
            size_t ssid_len = sizeof(wifi_config.sta.ssid);
            size_t pass_len = sizeof(wifi_config.sta.password);
            if (err == ESP_OK) {
                err = nvs_get_str(nvs_handle, "wifi_ssid", (char *)wifi_config.sta.ssid, &ssid_len);
                if (err == ESP_OK) {
                    ESP_LOGI(TAG, "Saved SSID: %s", wifi_config.sta.ssid);
                } else {
                    ESP_LOGE(TAG, "Failed to get SSID from NVS: %s", esp_err_to_name(err));
                }
                err = nvs_get_str(nvs_handle, "wifi_pass", (char *)wifi_config.sta.password, &pass_len);
                if (err == ESP_OK) {
                    ESP_LOGI(TAG, "Saved Password: %s", wifi_config.sta.password);
                } else {
                    ESP_LOGE(TAG, "Failed to get Password from NVS: %s", esp_err_to_name(err));
                }
                nvs_close(nvs_handle);
                if (strlen((char *)wifi_config.sta.ssid) > 0) {
                    wifi_config.sta.pmf_cfg.capable = true;
                    wifi_config.sta.pmf_cfg.required = false;
                    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
                    ESP_ERROR_CHECK(esp_wifi_connect());
                    return;
                }
            } else {
                ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
            }
        }
        ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH));
        smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_smartconfig_start(&cfg));
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGI(TAG, "Disconnect reason: %d", event->reason);
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        if (s_retry_num < MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retry to connect to the AP (attempt %d)", s_retry_num);
        } else {
            ESP_LOGI(TAG, "Connect to the AP fail");
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE) {
        ESP_LOGI(TAG, "Scan done");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL) {
        ESP_LOGI(TAG, "Found channel");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD) {
        ESP_LOGI(TAG, "Got SSID and password");

        smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
        wifi_config_t wifi_config;

        bzero(&wifi_config, sizeof(wifi_config_t));
        memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
        memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));

        nvs_handle_t nvs_handle;
        esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
        if (err == ESP_OK) {
            nvs_set_str(nvs_handle, "wifi_ssid", (char *)wifi_config.sta.ssid);
            nvs_set_str(nvs_handle, "wifi_pass", (char *)wifi_config.sta.password);
            nvs_commit(nvs_handle);
            nvs_close(nvs_handle);
            ESP_LOGI(TAG, "WiFi credentials saved to NVS");
        } else {
            ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        }

        ESP_LOGI(TAG, "SSID:%s", wifi_config.sta.ssid);
        ESP_LOGI(TAG, "PASSWORD:%s", wifi_config.sta.password);

        wifi_config.sta.pmf_cfg.capable = true;
        wifi_config.sta.pmf_cfg.required = false;

        ESP_ERROR_CHECK(esp_wifi_disconnect());
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_connect());
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE) {
        esptouch_done = true;
    }
}

static void smartconfig_task(void *parm) {
    s_wifi_event_group = xEventGroupCreate();

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .pull_down_en = 0,
        .pull_up_en = 1,
    };
    gpio_config(&io_conf);

    while (1) {
        if (esptouch_done) {
            xEventGroupSetBits(s_wifi_event_group, ESPTOUCH_DONE_BIT);
            esptouch_done = false;
        }

        if (gpio_get_level(BUTTON_GPIO) == 0) {
            vTaskDelay(pdMS_TO_TICKS(BUTTON_PRESS_TIMEOUT_MS));
            if (gpio_get_level(BUTTON_GPIO) == 0) {
                ESP_LOGI(TAG, "Button pressed, erasing WiFi config");
                erase_wifi_config();
                reset_wifi_config = true;
                esp_restart();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vEventGroupDelete(s_wifi_event_group);
    vTaskDelete(NULL);
}

void wifi_init(void) {
    xTaskCreate(smartconfig_task, "smartconfig_task", 4096, NULL, 5, NULL);
}
