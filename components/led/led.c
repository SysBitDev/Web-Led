#include "led.h"
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "led_strip_encoder.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "sdkconfig.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define RMT_LED_STRIP_RESOLUTION_HZ 10000000
#define RMT_LED_STRIP_GPIO_NUM CONFIG_RMT_LED_STRIP_GPIO_NUM
#define LED_NUMBERS CONFIG_LED_NUMBERS
#define CHASE_SPEED_MS CONFIG_CHASE_SPEED_MS
#define DELAY_STAIRS_MS CONFIG_DELAY_STAIRS_MS

static const char *TAG = "LED";
static rmt_channel_handle_t led_chan = NULL;
static rmt_encoder_handle_t led_encoder = NULL;
static TaskHandle_t effect_task_handle = NULL;
static int wave_direction = 1;
static uint8_t led_brightness = 100;
static uint8_t current_r = 255;
static uint8_t current_g = 255;
static uint8_t current_b = 255;
static bool custom_color_mode = false;

// Декларації функцій
static void wave_effect_task(void *arg);
static void stairs_effect_task(void *arg);
static void stairs_on_task(void *arg);
static void stairs_off_task(void *arg);
static void strip_motion_effect_1_task(void *arg);
static void strip_motion_effect_2_task(void *arg);
static void check_esp_error(esp_err_t err, const char* task);

static void check_esp_error(esp_err_t err, const char* task) {
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed in %s with error: %s", task, esp_err_to_name(err));
    }
}

static void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r, uint32_t *g, uint32_t *b) {
    h %= 360;
    float rgb_max = v * 2.55f;
    float rgb_min = rgb_max * (100 - s) / 100;
    int i = h / 60;
    float diff = h % 60;
    float rgb_adj = (rgb_max - rgb_min) * diff / 60;
    switch (i) {
        case 0: *r = rgb_max; *g = rgb_min + rgb_adj; *b = rgb_min; break;
        case 1: *r = rgb_max - rgb_adj; *g = rgb_max; *b = rgb_min; break;
        case 2: *r = rgb_min; *g = rgb_max; *b = rgb_min + rgb_adj; break;
        case 3: *r = rgb_min; *g = rgb_max - rgb_adj; *b = rgb_max; break;
        case 4: *r = rgb_min + rgb_adj; *g = rgb_min; *b = rgb_max; break;
        default: *r = rgb_max; *g = rgb_min; *b = rgb_max - rgb_adj; break;
    }
}

void led_strip_init(void) {
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = RMT_LED_STRIP_GPIO_NUM,
        .mem_block_symbols = 64,
        .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
        .trans_queue_depth = 4,
    };
    esp_err_t err = rmt_new_tx_channel(&tx_chan_config, &led_chan);
    check_esp_error(err, "LED strip TX channel initialization");
    led_strip_encoder_config_t encoder_config = {
        .resolution = RMT_LED_STRIP_RESOLUTION_HZ,
    };
    err = rmt_new_led_strip_encoder(&encoder_config, &led_encoder);
    check_esp_error(err, "LED strip encoder configuration");
    err = rmt_enable(led_chan);
    check_esp_error(err, "LED strip channel enable");
}

void led_strip_start(void) {
    ESP_LOGI(TAG, "Starting LED strip");
    uint32_t red, green, blue;
    uint16_t hue = 0;
    rmt_transmit_config_t tx_config = {.loop_count = 0};
    uint8_t led_strip_pixels[LED_NUMBERS * 3];
    memset(led_strip_pixels, 0, sizeof(led_strip_pixels));
    for (int i = 0; i < LED_NUMBERS; i++) {
        hue = i * 360 / LED_NUMBERS;
        if (custom_color_mode) {
            red = current_r * led_brightness / 100;
            green = current_g * led_brightness / 100;
            blue = current_b * led_brightness / 100;
        } else {
            led_strip_hsv2rgb(hue, 100, led_brightness, &red, &green, &blue);
        }
        led_strip_pixels[i * 3 + 0] = green;
        led_strip_pixels[i * 3 + 1] = blue;
        led_strip_pixels[i * 3 + 2] = red;
    }
    esp_err_t err = rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config);
    check_esp_error(err, "LED strip start transmission");
    err = rmt_tx_wait_all_done(led_chan, portMAX_DELAY);
    check_esp_error(err, "LED strip start wait");
}

void led_strip_stop(void) {
    ESP_LOGI(TAG, "Stopping LED strip");
    uint8_t led_strip_pixels[LED_NUMBERS * 3];
    memset(led_strip_pixels, 0, sizeof(led_strip_pixels));
    rmt_transmit_config_t tx_config = {.loop_count = 0};
    esp_err_t err = rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config);
    check_esp_error(err, "LED strip stop transmission");
    err = rmt_tx_wait_all_done(led_chan, portMAX_DELAY);
    check_esp_error(err, "LED strip stop wait");
}

void led_strip_wave_effect(void) {
    if (effect_task_handle != NULL) {
        vTaskDelete(effect_task_handle);
    }
    xTaskCreate(wave_effect_task, "wave_effect_task", 4096, NULL, 5, &effect_task_handle);
    ESP_LOGI(TAG, "Wave effect task started");
}

void led_strip_stairs_effect(void) {
    if (effect_task_handle != NULL) {
        vTaskDelete(effect_task_handle);
    }
    xTaskCreate(stairs_effect_task, "stairs_effect_task", 4096, NULL, 5, &effect_task_handle);
    ESP_LOGI(TAG, "Stairs effect task started");
}

void led_strip_stop_effect(void) {
    if (effect_task_handle != NULL) {
        vTaskDelete(effect_task_handle);
        effect_task_handle = NULL;
    }
    led_strip_stop();
    ESP_LOGI(TAG, "All effects stopped");
}

void led_strip_toggle_wave_direction(void) {
    wave_direction *= -1;
    ESP_LOGI(TAG, "Wave direction toggled to %d", wave_direction);
}

void led_strip_set_brightness(uint8_t brightness) {
    led_brightness = brightness;
    ESP_LOGI(TAG, "Brightness set to %d%%", brightness);
    led_strip_start();
}

void led_strip_set_color(uint8_t r, uint8_t g, uint8_t b) {
    custom_color_mode = true;
    current_r = r;
    current_g = g;
    current_b = b;
    ESP_LOGI(TAG, "Custom color set to R:%d, G:%d, B:%d", r, g, b);
    led_strip_start();
}

void led_strip_reset_to_rgb(void) {
    custom_color_mode = false;
    ESP_LOGI(TAG, "LED strip reset to RGB mode");
    led_strip_start();
}

uint8_t led_strip_get_brightness(void) {
    return led_brightness;
}

void led_strip_get_color(uint8_t *r, uint8_t *g, uint8_t *b) {
    if (r) *r = current_r;
    if (g) *g = current_g;
    if (b) *b = current_b;
}

bool led_strip_get_custom_color_mode(void) {
    return custom_color_mode;
}

void led_strip_save_parameters(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("led_storage", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        nvs_set_u8(nvs_handle, "led_brightness", led_brightness);
        uint8_t custom_color_mode_u8 = custom_color_mode ? 1 : 0;
        nvs_set_u8(nvs_handle, "custom_color_mode", custom_color_mode_u8);
        nvs_set_u8(nvs_handle, "current_r", current_r);
        nvs_set_u8(nvs_handle, "current_g", current_g);
        nvs_set_u8(nvs_handle, "current_b", current_b);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        ESP_LOGI(TAG, "LED parameters saved to NVS");
        ESP_LOGI(TAG, "Saved Parameters: Brightness: %d, Color Mode: %s, R:%d G:%d B:%d",
                 led_brightness, custom_color_mode ? "Custom" : "RGB", current_r, current_g, current_b);
    } else {
        ESP_LOGE(TAG, "Error opening NVS handle for writing: %s", esp_err_to_name(err));
    }
}

void led_strip_load_parameters(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("led_storage", NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        nvs_get_u8(nvs_handle, "led_brightness", &led_brightness);
        uint8_t custom_color_mode_u8 = 0;
        nvs_get_u8(nvs_handle, "custom_color_mode", &custom_color_mode_u8);
        custom_color_mode = (bool)custom_color_mode_u8;
        nvs_get_u8(nvs_handle, "current_r", &current_r);
        nvs_get_u8(nvs_handle, "current_g", &current_g);
        nvs_get_u8(nvs_handle, "current_b", &current_b);
        nvs_close(nvs_handle);
        ESP_LOGI(TAG, "LED parameters loaded from NVS");
        ESP_LOGI(TAG, "Loaded Parameters: Brightness: %d, Color Mode: %s, R:%d G:%d B:%d",
                 led_brightness, custom_color_mode ? "Custom" : "RGB", current_r, current_g, current_b);
    } else {
        ESP_LOGE(TAG, "Error opening NVS handle for reading: %s", esp_err_to_name(err));
    }
}

void led_strip_stairs_on(void) {
    xTaskCreate(stairs_on_task, "stairs_on_task", 4096, NULL, 5, NULL);
}

void led_strip_stairs_off(void) {
    xTaskCreate(stairs_off_task, "stairs_off_task", 4096, NULL, 5, NULL);
}

void led_strip_motion_effect_1(void) {
    if (effect_task_handle != NULL) {
        vTaskDelete(effect_task_handle);
    }
    xTaskCreate(strip_motion_effect_1_task, "motion_effect_1_task", 4096, NULL, 5, &effect_task_handle);
    ESP_LOGI(TAG, "Motion effect 1 task started");
}

void led_strip_motion_effect_2(void) {
    if (effect_task_handle != NULL) {
        vTaskDelete(effect_task_handle);
    }
    xTaskCreate(strip_motion_effect_2_task, "motion_effect_2_task", 4096, NULL, 5, &effect_task_handle);
    ESP_LOGI(TAG, "Motion effect 2 task started");
}

static void strip_motion_effect_1_task(void *arg) {
    ESP_LOGI(TAG, "Executing motion effect 1");
    uint32_t red, green, blue;
    uint16_t hue = 0;
    rmt_transmit_config_t tx_config = {.loop_count = 0};
    uint8_t led_strip_pixels[LED_NUMBERS * 3];

    for (int j = LED_NUMBERS - 1; j >= 0; j--) {
        memset(led_strip_pixels, 0, sizeof(led_strip_pixels));
        for (int i = LED_NUMBERS - 1; i >= j; i--) {
            hue = (LED_NUMBERS - 1 - i) * 360 / LED_NUMBERS;
            if (custom_color_mode) {
                red = current_r * led_brightness / 100;
                green = current_g * led_brightness / 100;
                blue = current_b * led_brightness / 100;
            } else {
                led_strip_hsv2rgb(hue, 100, led_brightness, &red, &green, &blue);
            }
            led_strip_pixels[i * 3 + 0] = green;
            led_strip_pixels[i * 3 + 1] = red;
            led_strip_pixels[i * 3 + 2] = blue;
        }
        esp_err_t err = rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config);
        check_esp_error(err, "Motion effect 1 transmission");
        err = rmt_tx_wait_all_done(led_chan, portMAX_DELAY);
        check_esp_error(err, "Motion effect 1 wait");
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    vTaskDelay(pdMS_TO_TICKS(DELAY_STAIRS_MS));

    for (int j = LED_NUMBERS - 1; j >= 0; j--) {
        for (int i = LED_NUMBERS - 1; i > j; i--) {
            led_strip_pixels[i * 3 + 0] = 0;
            led_strip_pixels[i * 3 + 1] = 0;
            led_strip_pixels[i * 3 + 2] = 0;
        }
        esp_err_t err = rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config);
        check_esp_error(err, "Motion effect 1 clear");
        err = rmt_tx_wait_all_done(led_chan, portMAX_DELAY);
        check_esp_error(err, "Motion effect 1 clear wait");
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    memset(led_strip_pixels, 0, sizeof(led_strip_pixels));
    esp_err_t err = rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config);
    check_esp_error(err, "Motion effect 1 final clear");
    err = rmt_tx_wait_all_done(led_chan, portMAX_DELAY);
    check_esp_error(err, "Motion effect 1 final clear wait");
    vTaskSuspend(NULL);
}

static void strip_motion_effect_2_task(void *arg) {
    ESP_LOGI(TAG, "Executing motion effect 2");
    uint32_t red, green, blue;
    uint16_t hue = 0;
    rmt_transmit_config_t tx_config = {.loop_count = 0};
    uint8_t led_strip_pixels[LED_NUMBERS * 3];

    for (int j = 0; j < LED_NUMBERS; j++) {
        memset(led_strip_pixels, 0, sizeof(led_strip_pixels));
        for (int i = 0; i <= j; i++) {
            hue = i * 360 / LED_NUMBERS;
            if (custom_color_mode) {
                red = current_r * led_brightness / 100;
                green = current_g * led_brightness / 100;
                blue = current_b * led_brightness / 100;
            } else {
                led_strip_hsv2rgb(hue, 100, led_brightness, &red, &green, &blue);
            }
            led_strip_pixels[i * 3 + 0] = green;
            led_strip_pixels[i * 3 + 1] = red;
            led_strip_pixels[i * 3 + 2] = blue;
        }
        esp_err_t err = rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config);
        check_esp_error(err, "Motion effect 2 light up transmission");
        err = rmt_tx_wait_all_done(led_chan, portMAX_DELAY);
        check_esp_error(err, "Motion effect 2 light up wait");
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    vTaskDelay(pdMS_TO_TICKS(DELAY_STAIRS_MS));

    for (int j = 0; j < LED_NUMBERS; j++) {
        for (int i = 0; i <= j; i++) {
            led_strip_pixels[i * 3 + 0] = 0;
            led_strip_pixels[i * 3 + 1] = 0;
            led_strip_pixels[i * 3 + 2] = 0;
        }
        esp_err_t err = rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config);
        check_esp_error(err, "Motion effect 2 turn off transmission");
        err = rmt_tx_wait_all_done(led_chan, portMAX_DELAY);
        check_esp_error(err, "Motion effect 2 turn off wait");
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    memset(led_strip_pixels, 0, sizeof(led_strip_pixels));
    esp_err_t err = rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config);
    check_esp_error(err, "Motion effect 2 final clear");
    err = rmt_tx_wait_all_done(led_chan, portMAX_DELAY);
    check_esp_error(err, "Motion effect 2 final clear wait");
    vTaskSuspend(NULL);
}

static void wave_effect_task(void *arg) {
    ESP_LOGI(TAG, "Running LED wave effect");
    uint32_t red, green, blue;
    uint16_t hue = 0;
    int16_t offset = 0;
    rmt_transmit_config_t tx_config = {.loop_count = 0};
    uint8_t led_strip_pixels[LED_NUMBERS * 3];
    int group_size = 5;
    const uint8_t min_brightness = led_brightness;
    while (true) {
        for (int i = 0; i < LED_NUMBERS; i++) {
            hue = ((i + offset) % LED_NUMBERS) * 360 / LED_NUMBERS;
            uint8_t brightness = (sin((float)(i % group_size) / group_size * 2 * M_PI) + 1) * (led_brightness / 2.0);
            brightness = brightness < min_brightness ? min_brightness : brightness;
            led_strip_hsv2rgb(hue, 255, brightness, &red, &green, &blue);
            led_strip_pixels[i * 3 + 0] = green;
            led_strip_pixels[i * 3 + 1] = red;
            led_strip_pixels[i * 3 + 2] = blue;
        }
        esp_err_t err = rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config);
        check_esp_error(err, "Wave effect transmission");
        err = rmt_tx_wait_all_done(led_chan, portMAX_DELAY);
        check_esp_error(err, "Wave effect wait");
        vTaskDelay(pdMS_TO_TICKS(CHASE_SPEED_MS));
        offset += wave_direction;
    }
}

static void stairs_effect_task(void *arg) {
    ESP_LOGI(TAG, "Running LED stairs effect");
    uint32_t red, green, blue;
    uint16_t hue = 0;
    rmt_transmit_config_t tx_config = {.loop_count = 0};
    uint8_t led_strip_pixels[LED_NUMBERS * 3];
    while (true) {
        for (int j = 0; j < LED_NUMBERS; j++) {
            memset(led_strip_pixels, 0, sizeof(led_strip_pixels));
            for (int i = 0; i <= j; i++) {
                hue = i * 360 / LED_NUMBERS;
                if (custom_color_mode) {
                    red = current_r * led_brightness / 100;
                    green = current_g * led_brightness / 100;
                    blue = current_b * led_brightness / 100;
                } else {
                    led_strip_hsv2rgb(hue, 100, led_brightness, &red, &green, &blue);
                }
                led_strip_pixels[i * 3 + 0] = green;
                led_strip_pixels[i * 3 + 1] = red;
                led_strip_pixels[i * 3 + 2] = blue;
            }
            esp_err_t err = rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config);
            check_esp_error(err, "Stairs effect ascend transmission");
            err = rmt_tx_wait_all_done(led_chan, portMAX_DELAY);
            check_esp_error(err, "Stairs effect ascend wait");
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        for (int j = LED_NUMBERS - 1; j >= 0; j--) {
            memset(led_strip_pixels, 0, sizeof(led_strip_pixels));
            for (int i = 0; i <= j; i++) {
                hue = i * 360 / LED_NUMBERS;
                if (custom_color_mode) {
                    red = current_r * led_brightness / 100;
                    green = current_g * led_brightness / 100;
                    blue = current_b * led_brightness / 100;
                } else {
                    led_strip_hsv2rgb(hue, 100, led_brightness, &red, &green, &blue);
                }
                led_strip_pixels[i * 3 + 0] = green;
                led_strip_pixels[i * 3 + 1] = red;
                led_strip_pixels[i * 3 + 2] = blue;
            }
            esp_err_t err = rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config);
            check_esp_error(err, "Stairs effect descend transmission");
            err = rmt_tx_wait_all_done(led_chan, portMAX_DELAY);
            check_esp_error(err, "Stairs effect descend wait");
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

static void stairs_on_task(void *arg) {
    uint32_t red, green, blue;
    uint16_t hue = 0;
    rmt_transmit_config_t tx_config = {.loop_count = 0};
    uint8_t led_strip_pixels[LED_NUMBERS * 3];
    memset(led_strip_pixels, 0, sizeof(led_strip_pixels));
    for (int j = 0; j < LED_NUMBERS; j++) {
        if (custom_color_mode) {
            red = current_r * led_brightness / 100;
            green = current_g * led_brightness / 100;
            blue = current_b * led_brightness / 100;
        } else {
            hue = j * 360 / LED_NUMBERS;
            led_strip_hsv2rgb(hue, 100, led_brightness, &red, &green, &blue);
        }
        led_strip_pixels[j * 3 + 0] = green;
        led_strip_pixels[j * 3 + 1] = red;
        led_strip_pixels[j * 3 + 2] = blue;
        esp_err_t err = rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config);
        check_esp_error(err, "Stairs on transmission");
        err = rmt_tx_wait_all_done(led_chan, portMAX_DELAY);
        check_esp_error(err, "Stairs on wait");
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelete(NULL);
}

static void stairs_off_task(void *arg) {
    rmt_transmit_config_t tx_config = {.loop_count = 0};
    uint8_t led_strip_pixels[LED_NUMBERS * 3];
    for (int j = LED_NUMBERS - 1; j >= 0; j--) {
        memset(led_strip_pixels, 0, sizeof(led_strip_pixels));
        for (int i = 0; i <= j; i++) {
            if (custom_color_mode) {
                led_strip_pixels[i * 3 + 0] = current_g * led_brightness / 100;
                led_strip_pixels[i * 3 + 1] = current_r * led_brightness / 100;
                led_strip_pixels[i * 3 + 2] = current_b * led_brightness / 100;
            } else {
                uint32_t red, green, blue;
                uint16_t hue = i * 360 / LED_NUMBERS;
                led_strip_hsv2rgb(hue, 100, led_brightness, &red, &green, &blue);
                led_strip_pixels[i * 3 + 0] = green;
                led_strip_pixels[i * 3 + 1] = red;
                led_strip_pixels[i * 3 + 2] = blue;
            }
        }
        esp_err_t err = rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config);
        check_esp_error(err, "Stairs off transmission");
        err = rmt_tx_wait_all_done(led_chan, portMAX_DELAY);
        check_esp_error(err, "Stairs off wait");
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    led_strip_stop();
    vTaskDelete(NULL);
}
