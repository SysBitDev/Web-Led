#include "led.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <math.h>
#include <string.h>
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
static bool effect_running = false;
static bool rgb_mode = false;

typedef enum {
    EFFECT_DIRECTION_START = 0,
    EFFECT_DIRECTION_END,
    EFFECT_DIRECTION_BOTH
} effect_direction_t;

static void led_strip_effect_task(void *arg);
static void led_strip_stairs_effect_task(void *arg);
static void set_all_leds(uint8_t r, uint8_t g, uint8_t b);
static esp_err_t led_strip_deinit(void);
static esp_err_t led_strip_init_with_length(uint16_t length);

void hsv_2_rgb(float h, float s, float v, uint8_t *r, uint8_t *g, uint8_t *b) {
    float c = v * s;
    float x = c * (1 - fabsf(fmodf(h / 60.0, 2) - 1));
    float m = v - c;
    float r_f, g_f, b_f;

    if (h >= 0 && h < 60) {
        r_f = c; g_f = x; b_f = 0;
    } else if (h >= 60 && h < 120) {
        r_f = x; g_f = c; b_f = 0;
    } else if (h >= 120 && h < 180) {
        r_f = 0; g_f = c; b_f = x;
    } else if (h >= 180 && h < 240) {
        r_f = 0; g_f = x; b_f = c;
    } else if (h >= 240 && h < 300) {
        r_f = x; g_f = 0; b_f = c;
    } else {
        r_f = c; g_f = 0; b_f = x;
    }

    *r = (uint8_t)((r_f + m) * 255);
    *g = (uint8_t)((g_f + m) * 255);
    *b = (uint8_t)((b_f + m) * 255);
}

void led_strip_init(void) {
    ESP_LOGI(TAG, "Initializing LED strip");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    led_mutex = xSemaphoreCreateMutex();
    if (led_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create led_mutex");
        return;
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
        .resolution_hz = 20 * 1000 * 1000,
        .mem_block_symbols = 64 * 3,
    };

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create new RMT device: %s", esp_err_to_name(err));
        return;
    }

    led_strip_load_parameters();
    set_all_leds(0, 0, 0);
}


static void set_all_leds(uint8_t r, uint8_t g, uint8_t b) {
    if (led_strip == NULL) {
        ESP_LOGE(TAG, "LED strip not initialized");
        return;
    }

    bool turn_off = (r == 0) && (g == 0) && (b == 0);

    if (turn_off) {
        ESP_ERROR_CHECK(led_strip_clear(led_strip));
    } else if (rgb_mode) {
        for (int i = 0; i < led_strip_length; i++) {
            float hue = ((float)i / led_strip_length) * 360.0f;
            uint8_t hr, hg, hb;
            hsv_2_rgb(hue, 1.0f, brightness / 100.0f, &hr, &hg, &hb);
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, i, hr, hg, hb));
        }
    } else {
        uint8_t adj_r = (uint8_t)((r * brightness) / 100);
        uint8_t adj_g = (uint8_t)((g * brightness) / 100);
        uint8_t adj_b = (uint8_t)((b * brightness) / 100);

        for (int i = 0; i < led_strip_length; i++) {
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, i, adj_r, adj_g, adj_b));
        }
    }

    ESP_ERROR_CHECK(led_strip_refresh(led_strip));
}



void led_strip_start(void) {
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
        if (rgb_mode) {
            set_all_leds(color_r, color_g, color_b);
        } else {
            custom_color_mode = true;
            set_all_leds(color_r, color_g, color_b);
        }
        xSemaphoreGive(led_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take led_mutex in led_strip_start");
    }
}

void led_strip_stop(void) {
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
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

esp_err_t led_strip_set_length(uint16_t count) {
    ESP_LOGI(TAG, "Setting LED strip length to %d", count);
    
    led_strip_stop_effect();
    
    esp_err_t ret = led_strip_deinit();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deinitialize LED strip: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = led_strip_init_with_length(count);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize LED strip with count %d: %s", count, esp_err_to_name(ret));
        return ret;
    }
    
    led_strip_load_parameters();
    
    ESP_LOGI(TAG, "LED strip length set to %d successfully", count);
    return ESP_OK;
}

static esp_err_t led_strip_deinit(void) {
    if (led_strip) {
        ESP_ERROR_CHECK(led_strip_clear(led_strip));
        ESP_ERROR_CHECK(led_strip_del(led_strip));
        led_strip = NULL;
        ESP_LOGI(TAG, "LED strip deinitialized");
    }
    return ESP_OK;
}



static esp_err_t led_strip_init_with_length(uint16_t length) {
    led_strip_length = length;

    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO,
        .max_leds = led_strip_length,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 20 * 1000 * 1000,
        .mem_block_symbols = 64 * 3,
    };

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create new RMT device with length %d: %s", length, esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}


uint16_t led_strip_get_length(void) {
    return led_strip_length;
}

bool led_strip_is_effect_running(void) {
    return effect_running;
}

void led_strip_set_color(uint8_t r, uint8_t g, uint8_t b) {
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
        color_r = r;
        color_g = g;
        color_b = b;
        custom_color_mode = true;
        rgb_mode = false;
        set_all_leds(color_r, color_g, color_b);
        xSemaphoreGive(led_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take led_mutex in set_color");
    }
}


void led_strip_set_rgb_mode(bool enable) {
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
        rgb_mode = enable;
        if (enable) {
            custom_color_mode = false;
        }
        set_all_leds(color_r, color_g, color_b);
        xSemaphoreGive(led_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take led_mutex in set_rgb_mode");
    }
}

bool led_strip_get_rgb_mode(void) {
    return rgb_mode;
}

void led_strip_reset_to_rgb(void) {
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
        rgb_mode = true;
        custom_color_mode = false;
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

            uint8_t adj_r, adj_g, adj_b;

            if (rgb_mode) {
                float hue = ((float)index / led_strip_length) * 360.0;
                hsv_2_rgb(hue, 1.0, brightness / 100.0, &adj_r, &adj_g, &adj_b);
            } else {
                adj_r = (uint8_t)((color_r * brightness) / 100);
                adj_g = (uint8_t)((color_g * brightness) / 100);
                adj_b = (uint8_t)((color_b * brightness) / 100);
            }

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
    effect_direction_t direction = (effect_direction_t)arg;
    int steps = led_strip_length;

    uint8_t *led_r = calloc(steps, sizeof(uint8_t));
    uint8_t *led_g = calloc(steps, sizeof(uint8_t));
    uint8_t *led_b = calloc(steps, sizeof(uint8_t));

    if (!led_r || !led_g || !led_b) {
        ESP_LOGE(TAG, "Failed to allocate memory for LED state arrays");
        free(led_r);
        free(led_g);
        free(led_b);
        vTaskDelete(NULL);
        return;
    }

    uint8_t adj_r = (uint8_t)((color_r * brightness) / 100);
    uint8_t adj_g = (uint8_t)((color_g * brightness) / 100);
    uint8_t adj_b = (uint8_t)((color_b * brightness) / 100);

    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
        ESP_ERROR_CHECK(led_strip_clear(led_strip));
        xSemaphoreGive(led_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take led_mutex in stairs_effect_task");
        free(led_r);
        free(led_g);
        free(led_b);
        vTaskDelete(NULL);
        return;
    }

    switch (direction) {
        case EFFECT_DIRECTION_START:
            for (int i = 0; i < steps; i += stairs_group_size) {
                if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
                    for (int j = 0; j < stairs_group_size && (i + j) < steps; j++) {
                        int idx = i + j;
                        uint8_t hr = adj_r;
                        uint8_t hg = adj_g;
                        uint8_t hb = adj_b;
                        if (rgb_mode) {
                            float hue = ((float)idx / led_strip_length) * 360.0f;
                            hsv_2_rgb(hue, 1.0f, brightness / 100.0f, &hr, &hg, &hb);
                        }
                        led_r[idx] = hr;
                        led_g[idx] = hg;
                        led_b[idx] = hb;
                    }

                    for (int k = 0; k < steps; k++) {
                        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, k, led_r[k], led_g[k], led_b[k]));
                    }
                    ESP_ERROR_CHECK(led_strip_refresh(led_strip));
                    xSemaphoreGive(led_mutex);
                } else {
                    ESP_LOGE(TAG, "Failed to take led_mutex during lighting up");
                }
                vTaskDelay(pdMS_TO_TICKS(stairs_speed));
            }
            break;

        case EFFECT_DIRECTION_END:
            for (int i = steps - 1; i >= 0; i -= stairs_group_size) {
                if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
                    for (int j = 0; j < stairs_group_size && (i - j) >= 0; j++) {
                        int idx = i - j;
                        uint8_t hr = adj_r;
                        uint8_t hg = adj_g;
                        uint8_t hb = adj_b;
                        if (rgb_mode) {
                            float hue = ((float)idx / led_strip_length) * 360.0f;
                            hsv_2_rgb(hue, 1.0f, brightness / 100.0f, &hr, &hg, &hb);
                        }
                        led_r[idx] = hr;
                        led_g[idx] = hg;
                        led_b[idx] = hb;
                    }

                    for (int k = 0; k < steps; k++) {
                        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, k, led_r[k], led_g[k], led_b[k]));
                    }
                    ESP_ERROR_CHECK(led_strip_refresh(led_strip));
                    xSemaphoreGive(led_mutex);
                } else {
                    ESP_LOGE(TAG, "Failed to take led_mutex during lighting up");
                }
                vTaskDelay(pdMS_TO_TICKS(stairs_speed));
            }
            break;

        case EFFECT_DIRECTION_BOTH:
            {
                int start = 0;
                int end = steps - 1;

                while (start <= end) {
                    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
                        for (int j = 0; j < stairs_group_size; j++) {
                            if ((start + j) <= end) {
                                int idx_start = start + j;
                                uint8_t hr = adj_r;
                                uint8_t hg = adj_g;
                                uint8_t hb = adj_b;
                                if (rgb_mode) {
                                    float hue = ((float)idx_start / led_strip_length) * 360.0f;
                                    hsv_2_rgb(hue, 1.0f, brightness / 100.0f, &hr, &hg, &hb);
                                }
                                led_r[idx_start] = hr;
                                led_g[idx_start] = hg;
                                led_b[idx_start] = hb;
                            }
                            if ((end - j) >= start) {
                                int idx_end = end - j;
                                uint8_t hr = adj_r;
                                uint8_t hg = adj_g;
                                uint8_t hb = adj_b;
                                if (rgb_mode) {
                                    float hue = ((float)idx_end / led_strip_length) * 360.0f;
                                    hsv_2_rgb(hue, 1.0f, brightness / 100.0f, &hr, &hg, &hb);
                                }
                                led_r[idx_end] = hr;
                                led_g[idx_end] = hg;
                                led_b[idx_end] = hb;
                            }
                        }

                        for (int k = 0; k < steps; k++) {
                            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, k, led_r[k], led_g[k], led_b[k]));
                        }
                        ESP_ERROR_CHECK(led_strip_refresh(led_strip));
                        xSemaphoreGive(led_mutex);
                    } else {
                        ESP_LOGE(TAG, "Failed to take led_mutex during lighting up");
                    }

                    start += stairs_group_size;
                    end -= stairs_group_size;
                    vTaskDelay(pdMS_TO_TICKS(stairs_speed));
                }
            }
            break;
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    switch (direction) {
        case EFFECT_DIRECTION_START:
            for (int i = steps - 1; i >= 0; i -= stairs_group_size) {
                if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
                    for (int j = 0; j < stairs_group_size && (i - j) >= 0; j++) {
                        int idx = i - j;
                        led_r[idx] = 0;
                        led_g[idx] = 0;
                        led_b[idx] = 0;
                    }

                    for (int k = 0; k < steps; k++) {
                        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, k, led_r[k], led_g[k], led_b[k]));
                    }
                    ESP_ERROR_CHECK(led_strip_refresh(led_strip));
                    xSemaphoreGive(led_mutex);
                } else {
                    ESP_LOGE(TAG, "Failed to take led_mutex during turning off");
                }
                vTaskDelay(pdMS_TO_TICKS(stairs_speed));
            }
            break;

        case EFFECT_DIRECTION_END:
            for (int i = 0; i < steps; i += stairs_group_size) {
                if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
                    for (int j = 0; j < stairs_group_size && (i + j) < steps; j++) {
                        int idx = i + j;
                        led_r[idx] = 0;
                        led_g[idx] = 0;
                        led_b[idx] = 0;
                    }

                    for (int k = 0; k < steps; k++) {
                        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, k, led_r[k], led_g[k], led_b[k]));
                    }
                    ESP_ERROR_CHECK(led_strip_refresh(led_strip));
                    xSemaphoreGive(led_mutex);
                } else {
                    ESP_LOGE(TAG, "Failed to take led_mutex during turning off");
                }
                vTaskDelay(pdMS_TO_TICKS(stairs_speed));
            }
            break;

        case EFFECT_DIRECTION_BOTH:
            {
                int start = steps / 2;
                int end = start - 1;

                while (start >= 0 || end < steps) {
                    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
                        for (int j = 0; j < stairs_group_size; j++) {
                            if ((start - j) >= 0) {
                                int idx_start = start - j;
                                led_r[idx_start] = 0;
                                led_g[idx_start] = 0;
                                led_b[idx_start] = 0;
                            }
                            if ((end + j) < steps) {
                                int idx_end = end + j;
                                led_r[idx_end] = 0;
                                led_g[idx_end] = 0;
                                led_b[idx_end] = 0;
                            }
                        }

                        for (int k = 0; k < steps; k++) {
                            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, k, led_r[k], led_g[k], led_b[k]));
                        }
                        ESP_ERROR_CHECK(led_strip_refresh(led_strip));
                        xSemaphoreGive(led_mutex);
                    } else {
                        ESP_LOGE(TAG, "Failed to take led_mutex during turning off");
                    }

                    start -= stairs_group_size;
                    end += stairs_group_size;
                    vTaskDelay(pdMS_TO_TICKS(stairs_speed));
                }
            }
            break;
    }

    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
        custom_color_mode = false;
        set_all_leds(0, 0, 0);
        effect_task_handle = NULL;
        effect_running = false;
        xSemaphoreGive(led_mutex);
    }

    free(led_r);
    free(led_g);
    free(led_b);

    vTaskDelete(NULL);
}


void led_strip_stairs_effect(void) {
    led_strip_stairs_effect_from_start();
}

void led_strip_stairs_effect_from_start(void) {
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
        if (effect_task_handle != NULL) {
            vTaskDelete(effect_task_handle);
            effect_task_handle = NULL;
        }

        custom_color_mode = true;
        effect_running = true;

        xTaskCreate(led_strip_stairs_effect_task, "stairs_effect_start", 8192,
                    (void *)EFFECT_DIRECTION_START, configMAX_PRIORITIES - 1,
                    &effect_task_handle);
        xSemaphoreGive(led_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take led_mutex in stairs_effect_from_start");
    }
}

void led_strip_stairs_effect_from_end(void) {
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
        if (effect_task_handle != NULL) {
            vTaskDelete(effect_task_handle);
            effect_task_handle = NULL;
        }

        custom_color_mode = true;
        effect_running = true;

        xTaskCreate(led_strip_stairs_effect_task, "stairs_effect_end", 8192,
                    (void *)EFFECT_DIRECTION_END, configMAX_PRIORITIES - 1,
                    &effect_task_handle);
        xSemaphoreGive(led_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take led_mutex in stairs_effect_from_end");
    }
}

void led_strip_stairs_effect_both(void) {
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
        if (effect_task_handle != NULL) {
            vTaskDelete(effect_task_handle);
            effect_task_handle = NULL;
        }

        custom_color_mode = true;
        effect_running = true;

        xTaskCreate(led_strip_stairs_effect_task, "stairs_effect_both", 8192,
                    (void *)EFFECT_DIRECTION_BOTH, configMAX_PRIORITIES - 1,
                    &effect_task_handle);
        xSemaphoreGive(led_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take led_mutex in stairs_effect_both");
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
        effect_running = false;
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
            nvs_set_u8(nvs_handle, "rgb_mode", rgb_mode ? 1 : 0);
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
            nvs_get_u8(nvs_handle, "rgb_mode", &value);
            rgb_mode = (value != 0);
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