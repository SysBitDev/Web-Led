#include "led.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "led_strip.h"

static const char *TAG = "led_strip";

SemaphoreHandle_t led_mutex = NULL;

static led_strip_handle_t led_strip;
static TaskHandle_t effect_task_handle = NULL;

static uint8_t brightness = 10;
static uint16_t stairs_speed = 20;
static uint16_t stairs_group_size = 3;
static uint8_t color_r = 255, color_g = 255, color_b = 255;
static bool custom_color_mode = false;
static bool wave_direction = false;
static uint16_t led_strip_length = 470;

extern SemaphoreHandle_t led_mutex;

static void led_strip_effect_task(void *arg);
static void led_strip_stairs_effect_task(void *arg);
static void set_all_leds(uint8_t r, uint8_t g, uint8_t b);
void led_strip_stop_effect(void);

void led_strip_init(void) {
    ESP_LOGI(TAG, "Initializing LED strip");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    if (led_mutex == NULL) {
        led_mutex = xSemaphoreCreateMutex();
        if (led_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create led_mutex");
            return;
        }
    }

    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO,
        .max_leds = led_strip_length,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create new RMT device: %s", esp_err_to_name(err));
        return;
    }

    led_strip_load_parameters();
    set_all_leds(color_r, color_g, color_b);
}


static void set_all_leds(uint8_t r, uint8_t g, uint8_t b) {
    if (led_strip == NULL) {
        ESP_LOGE(TAG, "LED strip not initialized");
        return;
    }

    for (int i = 0; i < led_strip_length; i++) {
        uint8_t adj_r = (uint8_t)((r * brightness) / 100);
        uint8_t adj_g = (uint8_t)((g * brightness) / 100);
        uint8_t adj_b = (uint8_t)((b * brightness) / 100);
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, i, adj_r, adj_g, adj_b));
    }
    ESP_ERROR_CHECK(led_strip_refresh(led_strip));
}

void led_strip_start(void) {
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
        custom_color_mode = true;
        set_all_leds(color_r, color_g, color_b);
        xSemaphoreGive(led_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take led_mutex in led_strip_start");
    }
}

void led_strip_stop(void) {
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
        custom_color_mode = false;
        set_all_leds(0, 0, 0);
        xSemaphoreGive(led_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take led_mutex in led_strip_stop");
    }
}

void led_strip_set_brightness(uint8_t new_brightness) {
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
        brightness = new_brightness;
        if (custom_color_mode) {
            set_all_leds(color_r, color_g, color_b);
        }
        xSemaphoreGive(led_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take led_mutex in set_brightness");
    }
}

void led_strip_set_length(uint16_t length) {
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
        led_strip_length = length;
        if (led_strip != NULL) {
            led_strip_clear(led_strip);
            led_strip_stop();
            led_strip_init();
        }
        xSemaphoreGive(led_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take led_mutex in set_length");
    }
}

uint16_t led_strip_get_length(void) {
    return led_strip_length;
}

void led_strip_set_color(uint8_t r, uint8_t g, uint8_t b) {
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
        color_r = r;
        color_g = g;
        color_b = b;
        custom_color_mode = true;
        set_all_leds(color_r, color_g, color_b);
        xSemaphoreGive(led_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take led_mutex in set_color");
    }
}

void led_strip_reset_to_rgb(void) {
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
        custom_color_mode = false;
        color_r = 255;
        color_g = 255;
        color_b = 255;
        set_all_leds(color_r, color_g, color_b);
        xSemaphoreGive(led_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take led_mutex in reset_to_rgb");
    }
}

uint8_t led_strip_get_brightness(void) {
    return brightness;
}

uint16_t led_strip_get_stairs_speed(void) {
    return stairs_speed;
}

void led_strip_set_stairs_speed(uint16_t speed) {
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
        stairs_speed = speed;
        xSemaphoreGive(led_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take led_mutex in set_stairs_speed");
    }
}

uint16_t led_strip_get_stairs_group_size(void) {
    return stairs_group_size;
}

void led_strip_set_stairs_group_size(uint16_t size) {
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
        if (size < 1) {
            stairs_group_size = 1;
        } else if (size > led_strip_length) {
            stairs_group_size = led_strip_length;
        } else {
            stairs_group_size = size;
        }
        xSemaphoreGive(led_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take led_mutex in set_stairs_group_size");
    }
}

void led_strip_get_color(uint8_t *r, uint8_t *g, uint8_t *b) {
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
        *r = color_r;
        *g = color_g;
        *b = color_b;
        xSemaphoreGive(led_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take led_mutex in get_color");
    }
}

bool led_strip_get_custom_color_mode(void) {
    return custom_color_mode;
}

static void led_strip_effect_task(void *arg) {
    const int delay_ms = 50;
    int pos = 0;

    while (1) {
        if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
            if (led_strip == NULL) {
                ESP_LOGE(TAG, "LED strip not initialized in effect task");
                xSemaphoreGive(led_mutex);
                vTaskDelete(NULL);
                return;
            }

            ESP_ERROR_CHECK(led_strip_clear(led_strip));

            int index = wave_direction ? (led_strip_length - 1 - pos) : pos;
            uint8_t adj_r = (uint8_t)((color_r * brightness) / 100);
            uint8_t adj_g = (uint8_t)((color_g * brightness) / 100);
            uint8_t adj_b = (uint8_t)((color_b * brightness) / 100);

            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, index, adj_r, adj_g, adj_b));
            ESP_ERROR_CHECK(led_strip_refresh(led_strip));

            pos = (pos + 1) % led_strip_length;
            xSemaphoreGive(led_mutex);
        } else {
            ESP_LOGE(TAG, "Failed to take led_mutex in effect_task");
        }

        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

void led_strip_wave_effect(void) {
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
        if (effect_task_handle != NULL) {
            vTaskDelete(effect_task_handle);
            effect_task_handle = NULL;
        }

        custom_color_mode = true;
        xTaskCreate(led_strip_effect_task, "wave_effect", 2048, NULL, 5, &effect_task_handle);
        xSemaphoreGive(led_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take led_mutex in wave_effect");
    }
}

void led_strip_toggle_wave_direction(void) {
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
        wave_direction = !wave_direction;
        xSemaphoreGive(led_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take led_mutex in toggle_wave_direction");
    }
}

static void led_strip_stairs_effect_task(void *arg) {
    int steps = led_strip_length;
    int i = 0;

    if (xSemaphoreTake(led_mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take led_mutex in stairs_effect_task");
        vTaskDelete(NULL);
        return;
    }

    ESP_ERROR_CHECK(led_strip_clear(led_strip));
    xSemaphoreGive(led_mutex);

    while (i < steps) {
        if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
            for (int j = 0; j < stairs_group_size && (i + j) < steps; j++) {
                uint8_t adj_r = (uint8_t)((color_r * brightness) / 100);
                uint8_t adj_g = (uint8_t)((color_g * brightness) / 100);
                uint8_t adj_b = (uint8_t)((color_b * brightness) / 100);

                ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, i + j, adj_r, adj_g, adj_b));
            }
            ESP_ERROR_CHECK(led_strip_refresh(led_strip));
            xSemaphoreGive(led_mutex);
        } else {
            ESP_LOGE(TAG, "Failed to take led_mutex in stairs_effect_task during lighting up");
        }

        i += stairs_group_size;
        vTaskDelay(pdMS_TO_TICKS(stairs_speed));
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    i = 0;
    while (i < steps) {
        if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
            for (int j = 0; j < stairs_group_size && (i + j) < steps; j++) {
                ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, i + j, 0, 0, 0));
            }
            ESP_ERROR_CHECK(led_strip_refresh(led_strip));
            xSemaphoreGive(led_mutex);
        } else {
            ESP_LOGE(TAG, "Failed to take led_mutex in stairs_effect_task during turning off");
        }

        i += stairs_group_size;
        vTaskDelay(pdMS_TO_TICKS(stairs_speed));
    }

    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
        custom_color_mode = false;
        set_all_leds(0, 0, 0);
        effect_task_handle = NULL;
        xSemaphoreGive(led_mutex);
    }

    vTaskDelete(NULL);
}

void led_strip_stairs_effect(void) {
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
        if (effect_task_handle != NULL) {
            vTaskDelete(effect_task_handle);
            effect_task_handle = NULL;
        }

        custom_color_mode = true;
        xTaskCreate(led_strip_stairs_effect_task, "stairs_effect", 4096, NULL, 5, &effect_task_handle);
        xSemaphoreGive(led_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take led_mutex in stairs_effect");
    }
}

void led_strip_stop_effect(void) {
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
        if (effect_task_handle != NULL) {
            vTaskDelete(effect_task_handle);
            effect_task_handle = NULL;
        }
        custom_color_mode = false;
        set_all_leds(0, 0, 0);
        xSemaphoreGive(led_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take led_mutex in stop_effect");
    }
}

void led_strip_save_parameters(void) {
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
        nvs_handle_t nvs_handle;
        esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
        if (err == ESP_OK) {
            nvs_set_u8(nvs_handle, "brightness", brightness);
            nvs_set_u8(nvs_handle, "color_r", color_r);
            nvs_set_u8(nvs_handle, "color_g", color_g);
            nvs_set_u8(nvs_handle, "color_b", color_b);
            nvs_set_u8(nvs_handle, "custom_color", custom_color_mode ? 1 : 0);
            nvs_set_u16(nvs_handle, "stairs_speed", stairs_speed);
            nvs_set_u16(nvs_handle, "stairs_group_size", stairs_group_size);
            nvs_commit(nvs_handle);
            nvs_close(nvs_handle);
            ESP_LOGI(TAG, "Parameters saved");
        } else {
            ESP_LOGE(TAG, "Error saving parameters: %s", esp_err_to_name(err));
        }
        xSemaphoreGive(led_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take led_mutex in save_parameters");
    }
}

void led_strip_load_parameters(void) {
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
        nvs_handle_t nvs_handle;
        esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);
        if (err == ESP_OK) {
            uint8_t value = 0;
            nvs_get_u8(nvs_handle, "brightness", &brightness);
            nvs_get_u8(nvs_handle, "color_r", &color_r);
            nvs_get_u8(nvs_handle, "color_g", &color_g);
            nvs_get_u8(nvs_handle, "color_b", &color_b);
            nvs_get_u8(nvs_handle, "custom_color", &value);
            custom_color_mode = (value != 0);
            nvs_get_u16(nvs_handle, "stairs_speed", &stairs_speed);
            nvs_get_u16(nvs_handle, "stairs_group_size", &stairs_group_size);
            nvs_close(nvs_handle);
            ESP_LOGI(TAG, "Parameters loaded");
        } else {
            ESP_LOGE(TAG, "No parameters saved, using defaults");
        }
        xSemaphoreGive(led_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take led_mutex in load_parameters");
    }
}