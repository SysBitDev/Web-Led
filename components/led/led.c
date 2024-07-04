#include "led.h"
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "led_strip_encoder.h"
#include "sdkconfig.h"

// Define M_PI if not defined
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Constants for LED strip configuration
#define RMT_LED_STRIP_RESOLUTION_HZ 10000000 // 10MHz resolution, 1 tick = 0.1us
#define RMT_LED_STRIP_GPIO_NUM CONFIG_RMT_LED_STRIP_GPIO_NUM // GPIO number where the LED strip is connected
#define LED_NUMBERS CONFIG_LED_NUMBERS // Number of LEDs in the strip
#define CHASE_SPEED_MS CONFIG_CHASE_SPEED_MS // Speed of the chase effect
#define DELAY_STAIRS_MS CONFIG_DELAY_STAIRS_MS // Speed of the chase effect

// Logger tag for debugging
static const char *TAG = "LED";

// RMT channel and encoder handles
static rmt_channel_handle_t led_chan = NULL;
static rmt_encoder_handle_t led_encoder = NULL;

// Task handle for managing effects
static TaskHandle_t effect_task_handle = NULL;

// Direction of the wave, 1 for forward, -1 for backward
static int wave_direction = 1;

// Brightness of the LED strip (0-100)
static uint8_t led_brightness = 100;

// Current color values for custom color mode
static uint8_t current_r = 255;
static uint8_t current_g = 255;
static uint8_t current_b = 255;
static bool custom_color_mode = false;

/**
 * @brief Convert HSV color space to RGB color space
 *
 * @param h Hue value
 * @param s Saturation value
 * @param v Value (brightness)
 * @param r Pointer to store red component
 * @param g Pointer to store green component
 * @param b Pointer to store blue component
 */
void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r, uint32_t *g, uint32_t *b) {
    h %= 360; // Ensure hue is within [0, 360]
    uint32_t rgb_max = v * 2.55f;
    uint32_t rgb_min = rgb_max * (100 - s) / 100.0f;

    uint32_t i = h / 60;
    uint32_t diff = h % 60;

    // RGB adjustment amount by hue
    uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

    switch (i) {
    case 0:
        *r = rgb_max;
        *g = rgb_min + rgb_adj;
        *b = rgb_min;
        break;
    case 1:
        *r = rgb_max - rgb_adj;
        *g = rgb_max;
        *b = rgb_min;
        break;
    case 2:
        *r = rgb_min;
        *g = rgb_max;
        *b = rgb_min + rgb_adj;
        break;
    case 3:
        *r = rgb_min;
        *g = rgb_max - rgb_adj;
        *b = rgb_max;
        break;
    case 4:
        *r = rgb_min + rgb_adj;
        *g = rgb_min;
        *b = rgb_max;
        break;
    default:
        *r = rgb_max;
        *g = rgb_min;
        *b = rgb_max - rgb_adj;
        break;
    }
}

/**
 * @brief Initialize the LED strip
 */
void led_strip_init(void) {
    // Configure RMT TX channel
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = RMT_LED_STRIP_GPIO_NUM,
        .mem_block_symbols = 64,
        .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
        .trans_queue_depth = 4,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &led_chan));

    // Configure LED strip encoder
    led_strip_encoder_config_t encoder_config = {
        .resolution = RMT_LED_STRIP_RESOLUTION_HZ,
    };
    ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config, &led_encoder));

    // Enable RMT channel
    ESP_ERROR_CHECK(rmt_enable(led_chan));
}

void led_strip_start(void) {
    ESP_LOGI(TAG, "Start LED strip");
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

    ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
}

void led_strip_stop(void) {
    ESP_LOGI(TAG, "Stop LED strip");
    uint8_t led_strip_pixels[LED_NUMBERS * 3];
    memset(led_strip_pixels, 0, sizeof(led_strip_pixels));
    rmt_transmit_config_t tx_config = {.loop_count = 0};
    ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
}

/**
 * @brief Task to run the wave effect on the LED strip
 *
 * @param arg Task arguments (unused)
 */
static void wave_effect_task(void *arg) {
    ESP_LOGI(TAG, "Start LED wave effect");
    uint32_t red, green, blue;
    uint16_t hue = 0;
    int16_t offset = 0;  // Offset for wave effect
    rmt_transmit_config_t tx_config = {
        .loop_count = 0,
    };

    uint8_t led_strip_pixels[LED_NUMBERS * 3];
    int group_size = 5;  // Size of LED groups
    const uint8_t min_brightness = led_brightness;

    while (1) {
        for (int i = 0; i < LED_NUMBERS; i++) {
            hue = ((i + offset) % LED_NUMBERS) * 360 / LED_NUMBERS;
            uint8_t brightness = (sin((float)(i % group_size) / group_size * 2 * M_PI) + 1) * (led_brightness / 2.0);  // Brightness based on group size and led_brightness
            brightness = brightness < min_brightness ? min_brightness : brightness;

            led_strip_hsv2rgb(hue, 255, brightness, &red, &green, &blue);  // Always use RGB mode for wave effect

            led_strip_pixels[i * 3 + 0] = green;
            led_strip_pixels[i * 3 + 1] = red;
            led_strip_pixels[i * 3 + 2] = blue;
        }

        ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
        ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));

        vTaskDelay(pdMS_TO_TICKS(CHASE_SPEED_MS));  // Delay for smooth effect

        offset += wave_direction;  // Increment offset for continuous effect
    }
}

/**
 * @brief Task to run the stairs effect on the LED strip
 *
 * @param arg Task arguments (unused)
 */
static void stairs_effect_task(void *arg) {
    ESP_LOGI(TAG, "Start LED stairs effect");
    uint32_t red, green, blue;
    uint16_t hue = 0;
    rmt_transmit_config_t tx_config = {
        .loop_count = 0,
    };

    uint8_t led_strip_pixels[LED_NUMBERS * 3];
    while (1) {
        // Ascend
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
            ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
            ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
            vTaskDelay(pdMS_TO_TICKS(100)); // Increased delay for slower effect
        }
        // Descend
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
            ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
            ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
            vTaskDelay(pdMS_TO_TICKS(100)); // Increased delay for slower effect
        }
    }
}


static void strip_motion_effect_1_task(void *arg) {
    ESP_LOGI(TAG, "Running LED motion 1 effect 1 once");
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
        ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
        ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    vTaskDelay(pdMS_TO_TICKS(DELAY_STAIRS_MS));

    for (int j = LED_NUMBERS - 1; j >= 0; j--) {
        for (int i = LED_NUMBERS - 1; i > j; i--) {
            led_strip_pixels[i * 3 + 0] = 0;
            led_strip_pixels[i * 3 + 1] = 0;
            led_strip_pixels[i * 3 + 2] = 0;
        }
        ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
        ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    memset(led_strip_pixels, 0, sizeof(led_strip_pixels));
    ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));

    vTaskSuspend(NULL);
}

static void strip_motion_effect_2_task(void *arg) {
    ESP_LOGI(TAG, "Running LED motion 2 effect 1 once");
    uint32_t red, green, blue;
    uint16_t hue = 0;
    rmt_transmit_config_t tx_config = {
        .loop_count = 0,
    };
    uint8_t led_strip_pixels[LED_NUMBERS * 3];

        // Light up from the beginning to the end
    for (int j = 0; j < LED_NUMBERS; j++) {
        memset(led_strip_pixels, 0, sizeof(led_strip_pixels));
        for (int i = 0; i <= j; i++) {
            hue = i * 360 / LED_NUMBERS;  // Adjust hue for color effects from the start to the end
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
        ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
        ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    vTaskDelay(pdMS_TO_TICKS(DELAY_STAIRS_MS));

    // Turn off from the beginning to the end
    for (int j = 0; j < LED_NUMBERS; j++) {
        for (int i = 0; i <= j; i++) {
            led_strip_pixels[i * 3 + 0] = 0;
            led_strip_pixels[i * 3 + 1] = 0;
            led_strip_pixels[i * 3 + 2] = 0;
        }
        ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
        ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    memset(led_strip_pixels, 0, sizeof(led_strip_pixels));
    vTaskSuspend(NULL);
}

void led_strip_motion_effect_1(void) {
    if (effect_task_handle != NULL) {
        vTaskDelete(effect_task_handle);
    }
    xTaskCreate(strip_motion_effect_1_task, "strip_motion_effect_1_task", 2048, NULL, 5, &effect_task_handle);
}


void led_strip_motion_effect_2(void) {
    if (effect_task_handle != NULL) {
        vTaskDelete(effect_task_handle);
    }
    xTaskCreate(strip_motion_effect_2_task, "strip_motion_effect_2_task", 2048, NULL, 5, &effect_task_handle);
}


/**
 * @brief Start the wave effect
 */
void led_strip_wave_effect(void) {
    if (effect_task_handle != NULL) {
        vTaskDelete(effect_task_handle);
    }
    xTaskCreate(wave_effect_task, "wave_effect_task", 2048, NULL, 5, &effect_task_handle);
}

/**
 * @brief Start the stairs effect
 */
void led_strip_stairs_effect(void) {
    if (effect_task_handle != NULL) {
        vTaskDelete(effect_task_handle);
    }
    xTaskCreate(stairs_effect_task, "stairs_effect_task", 2048, NULL, 5, &effect_task_handle);
}


/**
 * @brief Change the direction of the wave effect.
 */
void led_strip_toggle_wave_direction(void) {
    wave_direction *= -1;
}

/**
 * @brief Stop all effects on the LED strip
 */
void led_strip_stop_effect(void) {
    if (effect_task_handle != NULL) {
        vTaskDelete(effect_task_handle);
        effect_task_handle = NULL;
    }
    led_strip_stop();
}

/**
 * @brief Sets the brightness of the LED strip.
 *
 * @param brightness Brightness level (0-100)
 */
void led_strip_set_brightness(uint8_t brightness) {
    led_brightness = brightness;
    ESP_LOGI(TAG, "Set brightness to %d", led_brightness);
}

/**
 * @brief Sets the color of the LED strip.
 *
 * @param r Red component
 * @param g Green component
 * @param b Blue component
 */
void led_strip_set_color(uint8_t r, uint8_t g, uint8_t b) {
    custom_color_mode = true;
    current_r = r;
    current_g = g;
    current_b = b;

    uint8_t led_strip_pixels[LED_NUMBERS * 3];
    rmt_transmit_config_t tx_config = {
        .loop_count = 0,
    };

    uint8_t adjusted_r = r * led_brightness / 100;
    uint8_t adjusted_g = g * led_brightness / 100;
    uint8_t adjusted_b = b * led_brightness / 100;

    for (int i = 0; i < LED_NUMBERS; i++) {
        led_strip_pixels[i * 3 + 0] = adjusted_g;
        led_strip_pixels[i * 3 + 1] = adjusted_r;
        led_strip_pixels[i * 3 + 2] = adjusted_b;
    }

    ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
}


/**
 * @brief Resets the LED strip to the RGB mode.
 */
void led_strip_reset_to_rgb(void) {
    custom_color_mode = false;
    led_strip_start();
}
