#include "led.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"

static const char *TAG = "led_strip";

static led_strip_handle_t led_strip;
static TaskHandle_t effect_task_handle = NULL;
static uint8_t brightness = 100;
static uint16_t stairs_speed = 100;
static uint16_t stairs_group_size = 1;
static uint8_t color_r = 255, color_g = 255, color_b = 255;
static bool custom_color_mode = false;
static bool wave_direction = false;

void led_strip_init(void) {
    ESP_LOGI(TAG, "Initializing LED strip");

    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO,
        .max_leds = LED_STRIP_LEN,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));

    led_strip_load_parameters();
}

static void set_all_leds(uint8_t r, uint8_t g, uint8_t b) {
    for (int i = 0; i < LED_STRIP_LEN; i++) {
        uint8_t adj_r = (uint8_t)((r * brightness) / 100);
        uint8_t adj_g = (uint8_t)((g * brightness) / 100);
        uint8_t adj_b = (uint8_t)((b * brightness) / 100);
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, i, adj_r, adj_g, adj_b));
    }
    ESP_ERROR_CHECK(led_strip_refresh(led_strip));
}

void led_strip_start(void) {
    custom_color_mode = true;
    set_all_leds(color_r, color_g, color_b);
}

void led_strip_stop(void) {
    custom_color_mode = false;
    set_all_leds(0, 0, 0);
}

void led_strip_set_brightness(uint8_t new_brightness) {
    brightness = new_brightness;
    if (custom_color_mode) {
        led_strip_start();
    }
}

void led_strip_set_color(uint8_t r, uint8_t g, uint8_t b) {
    color_r = r;
    color_g = g;
    color_b = b;
    custom_color_mode = true;
    led_strip_start();
}

void led_strip_reset_to_rgb(void) {
    custom_color_mode = false;
    color_r = 255;
    color_g = 255;
    color_b = 255;
    led_strip_start();
}

uint8_t led_strip_get_brightness(void) {
    return brightness;
}

uint16_t led_strip_get_stairs_speed(void) {
    return stairs_speed;
}

void led_strip_set_stairs_speed(uint16_t speed) {
    stairs_speed = speed;
}

uint16_t led_strip_get_stairs_group_size(void) {
    return stairs_group_size;
}

void led_strip_set_stairs_group_size(uint16_t size) {
    if (size < 1) {
        stairs_group_size = 1;
    } else if (size > LED_STRIP_LEN) {
        stairs_group_size = LED_STRIP_LEN;
    } else {
        stairs_group_size = size;
    }
}

void led_strip_get_color(uint8_t *r, uint8_t *g, uint8_t *b) {
    *r = color_r;
    *g = color_g;
    *b = color_b;
}

bool led_strip_get_custom_color_mode(void) {
    return custom_color_mode;
}

static void led_strip_effect_task(void *arg) {
    const int delay_ms = 50;
    int pos = 0;

    while (1) {
        ESP_ERROR_CHECK(led_strip_clear(led_strip));

        int index = wave_direction ? (LED_STRIP_LEN - 1 - pos) : pos;
        uint8_t adj_r = (uint8_t)((color_r * brightness) / 100);
        uint8_t adj_g = (uint8_t)((color_g * brightness) / 100);
        uint8_t adj_b = (uint8_t)((color_b * brightness) / 100);

        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, index, adj_r, adj_g, adj_b));
        ESP_ERROR_CHECK(led_strip_refresh(led_strip));

        pos = (pos + 1) % LED_STRIP_LEN;
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

void led_strip_wave_effect(void) {
    led_strip_stop_effect();
    custom_color_mode = true;
    xTaskCreate(led_strip_effect_task, "wave_effect", 2048, NULL, 5, &effect_task_handle);
}

void led_strip_toggle_wave_direction(void) {
    wave_direction = !wave_direction;
}

void led_strip_stop_effect(void) {
    if (effect_task_handle != NULL) {
        vTaskDelete(effect_task_handle);
        effect_task_handle = NULL;
    }
    led_strip_start();
}

static void led_strip_stairs_effect_task(void *arg) {
    int steps = LED_STRIP_LEN;
    int i = 0;

    ESP_ERROR_CHECK(led_strip_clear(led_strip));

    while (i < steps) {
        for (int j = 0; j < stairs_group_size && (i + j) < steps; j++) {
            uint8_t adj_r = (uint8_t)((color_r * brightness) / 100);
            uint8_t adj_g = (uint8_t)((color_g * brightness) / 100);
            uint8_t adj_b = (uint8_t)((color_b * brightness) / 100);

            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, i + j, adj_r, adj_g, adj_b));
        }
        ESP_ERROR_CHECK(led_strip_refresh(led_strip));
        i += stairs_group_size;
        vTaskDelay(pdMS_TO_TICKS(stairs_speed));
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    i = 0;
    while (i < steps) {
        for (int j = 0; j < stairs_group_size && (i + j) < steps; j++) {
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, i + j, 0, 0, 0));
        }
        ESP_ERROR_CHECK(led_strip_refresh(led_strip));
        i += stairs_group_size;
        vTaskDelay(pdMS_TO_TICKS(stairs_speed));
    }

    led_strip_stop_effect();
    vTaskDelete(NULL);
}




void led_strip_stairs_effect(void) {
    led_strip_stop_effect();
    custom_color_mode = true;
    xTaskCreate(led_strip_stairs_effect_task, "stairs_effect", 2048, NULL, 5, &effect_task_handle);
}

void led_strip_motion_effect_1(void) {
    led_strip_stairs_on();
}

void led_strip_motion_effect_2(void) {
    led_strip_wave_effect();
}

void led_strip_save_parameters(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        nvs_set_u8(nvs_handle, "brightness", brightness);
        nvs_set_u8(nvs_handle, "color_r", color_r);
        nvs_set_u8(nvs_handle, "color_g", color_g);
        nvs_set_u8(nvs_handle, "color_b", color_b);
        nvs_set_u8(nvs_handle, "custom_color", custom_color_mode);
        nvs_set_u16(nvs_handle, "stairs_speed", stairs_speed);
        nvs_set_u16(nvs_handle, "stairs_group_size", stairs_group_size);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        ESP_LOGI(TAG, "Parameters saved");
    } else {
        ESP_LOGE(TAG, "Error saving parameters");
    }
}

void led_strip_load_parameters(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        uint8_t value = 0;
        nvs_get_u8(nvs_handle, "brightness", &brightness);
        nvs_get_u8(nvs_handle, "color_r", &color_r);
        nvs_get_u8(nvs_handle, "color_g", &color_g);
        nvs_get_u8(nvs_handle, "color_b", &color_b);
        nvs_get_u8(nvs_handle, "custom_color", &value);
        custom_color_mode = value;
        nvs_get_u16(nvs_handle, "stairs_speed", &stairs_speed);
        nvs_get_u16(nvs_handle, "stairs_group_size", &stairs_group_size);
        nvs_close(nvs_handle);
        ESP_LOGI(TAG, "Parameters loaded");
    } else {
        ESP_LOGE(TAG, "No parameters saved, using defaults");
    }
}

void led_strip_stairs_on(void) {
    led_strip_stop_effect();
    custom_color_mode = true;
    xTaskCreate(led_strip_stairs_effect_task, "stairs_on", 2048, NULL, 5, &effect_task_handle);
}

static void led_strip_stairs_off_task(void *arg) {
    int steps = LED_STRIP_LEN;
    int delay_ms = 100;

    for (int i = steps - 1; i >= 0; i--) {
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, i, 0, 0, 0));
        ESP_ERROR_CHECK(led_strip_refresh(led_strip));
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }

    led_strip_stop();
    vTaskDelete(NULL);
}

void led_strip_stairs_off(void) {
    led_strip_stop_effect();
    xTaskCreate(led_strip_stairs_off_task, "stairs_off", 2048, NULL, 5, &effect_task_handle);
}
