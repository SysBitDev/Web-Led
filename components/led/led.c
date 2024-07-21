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

// Helper function to log errors
static void check_esp_error(esp_err_t err, const char* task) {
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed in %s with error: %d", task, err);
    }
}

/**
 * @brief Converts HSV values to RGB values.
 * @param h Hue angle (0-360 degrees)
 * @param s Saturation percentage (0-100)
 * @param v Value or brightness percentage (0-100)
 * @param r Pointer to store the computed red component
 * @param g Pointer to store the computed green component
 * @param b Pointer to store the computed blue component
 */
static void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r, uint32_t *g, uint32_t *b) {
    h %= 360; // Normalize hue to be within 0-359 degrees
    float rgb_max = v * 2.55f; // Convert brightness level to a scale of 0-255
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

/**
 * @brief Initializes the LED strip by setting up the RMT module and encoder.
 */
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

/**
 * @brief Starts the LED strip with the current settings (color and brightness).
 */
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

/**
 * @brief Stops the LED strip by clearing all LEDs.
 */
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

/**
 * @brief Task to run the wave effect on the LED strip.
 * This effect creates a moving wave pattern across the LEDs.
 * @param arg Task arguments (unused)
 */
static void wave_effect_task(void *arg) {
    ESP_LOGI(TAG, "Running LED wave effect");
    uint32_t red, green, blue;
    uint16_t hue = 0;
    int16_t offset = 0;  // Offset controls the starting point of the wave
    rmt_transmit_config_t tx_config = {.loop_count = 0};

    uint8_t led_strip_pixels[LED_NUMBERS * 3];
    int group_size = 5;  // Group size for wave segments
    const uint8_t min_brightness = led_brightness;

    while (true) {
        for (int i = 0; i < LED_NUMBERS; i++) {
            hue = ((i + offset) % LED_NUMBERS) * 360 / LED_NUMBERS;  // Adjust hue based on position and offset
            uint8_t brightness = (sin((float)(i % group_size) / group_size * 2 * M_PI) + 1) * (led_brightness / 2.0);  // Calculate brightness based on sine wave
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
        offset += wave_direction; // Move the wave based on the current direction
    }
}

/**
 * @brief Task to run the stairs effect on the LED strip.
 * This effect simulates a lighting pattern that ascends and descends like stairs.
 * @param arg Task arguments (unused)
 */
static void stairs_effect_task(void *arg) {
    ESP_LOGI(TAG, "Running LED stairs effect");
    uint32_t red, green, blue;
    uint16_t hue = 0;
    rmt_transmit_config_t tx_config = {.loop_count = 0};

    uint8_t led_strip_pixels[LED_NUMBERS * 3];
    while (true) {
        // Ascend: light up each LED one by one from start to end
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

        // Descend: turn off each LED one by one from end to start
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

/**
 * @brief Activates a specific motion effect to demonstrate or test LED configurations.
 * @param arg Task arguments (unused)
 */
static void strip_motion_effect_1_task(void *arg) {
    ESP_LOGI(TAG, "Executing motion effect 1");
    uint32_t red, green, blue;
    uint16_t hue = 0;
    rmt_transmit_config_t tx_config = {.loop_count = 0};
    uint8_t led_strip_pixels[LED_NUMBERS * 3];

    // Light up from the beginning to the end
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

    // Turn off from the beginning to the end
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

    // Clear the strip after the effect completes
    memset(led_strip_pixels, 0, sizeof(led_strip_pixels));
    esp_err_t err = rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config);
    check_esp_error(err, "Motion effect 1 final clear");
    err = rmt_tx_wait_all_done(led_chan, portMAX_DELAY);
    check_esp_error(err, "Motion effect 1 final clear wait");
    vTaskSuspend(NULL);  // Optionally suspend the task as it is designed to run once
}

/**
 * @brief Activates a different specific motion effect, providing a visual contrast to the first.
 * @param arg Task arguments (unused)
 */
static void strip_motion_effect_2_task(void *arg) {
    ESP_LOGI(TAG, "Executing motion effect 2");
    uint32_t red, green, blue;
    uint16_t hue = 0;
    rmt_transmit_config_t tx_config = {.loop_count = 0};
    uint8_t led_strip_pixels[LED_NUMBERS * 3];

    // Light up from the beginning to the end
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

    // Turn off from the beginning to the end
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

    // Clear the strip after the effect completes
    memset(led_strip_pixels, 0, sizeof(led_strip_pixels));
    esp_err_t err = rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config);
    check_esp_error(err, "Motion effect 2 final clear");
    err = rmt_tx_wait_all_done(led_chan, portMAX_DELAY);
    check_esp_error(err, "Motion effect 2 final clear wait");
    vTaskSuspend(NULL);  // Optionally suspend the task as it is designed to run once
}

/**
 * Start specific motion effect 1.
 */
void led_strip_motion_effect_1(void) {
    if (effect_task_handle != NULL) {
        vTaskDelete(effect_task_handle);
    }
    xTaskCreate(strip_motion_effect_1_task, "strip_motion_effect_1_task", 2048, NULL, 5, &effect_task_handle);
    ESP_LOGI(TAG, "Motion effect 1 task started");
}

/**
 * Start specific motion effect 2.
 */
void led_strip_motion_effect_2(void) {
    if (effect_task_handle != NULL) {
        vTaskDelete(effect_task_handle);
    }
    xTaskCreate(strip_motion_effect_2_task, "strip_motion_effect_2_task", 2048, NULL, 5, &effect_task_handle);
    ESP_LOGI(TAG, "Motion effect 2 task started");
}

/**
 * @brief Starts the predefined wave effect on the LED strip.
 */
void led_strip_wave_effect(void) {
    if (effect_task_handle != NULL) {
        vTaskDelete(effect_task_handle);
    }
    xTaskCreate(wave_effect_task, "wave_effect_task", 2048, NULL, 5, &effect_task_handle);
    ESP_LOGI(TAG, "Wave effect task started");
}

/**
 * @brief Starts the predefined stairs effect on the LED strip.
 */
void led_strip_stairs_effect(void) {
    if (effect_task_handle != NULL) {
        vTaskDelete(effect_task_handle);
    }
    xTaskCreate(stairs_effect_task, "stairs_effect_task", 2048, NULL, 5, &effect_task_handle);
    ESP_LOGI(TAG, "Stairs effect task started");
}

/**
 * @brief Toggles the direction of the wave effect.
 */
void led_strip_toggle_wave_direction(void) {
    wave_direction *= -1;
    ESP_LOGI(TAG, "Wave direction toggled to %d", wave_direction);
}

/**
 * @brief Stops all effects on the LED strip, reverting to initial state.
 */
void led_strip_stop_effect(void) {
    if (effect_task_handle != NULL) {
        vTaskDelete(effect_task_handle);
        effect_task_handle = NULL;
    }
    led_strip_stop();
    ESP_LOGI(TAG, "All effects stopped");
}

/**
 * @brief Sets the brightness level of the LED strip.
 * @param brightness New brightness level (0-100).
 */
void led_strip_set_brightness(uint8_t brightness) {
    led_brightness = brightness;
    ESP_LOGI(TAG, "Brightness set to %d%%", brightness);
    led_strip_start();  // Apply the new brightness to the active mode
}

/**
 * @brief Sets the custom color of the LED strip.
 * @param r Red component (0-255).
 * @param g Green component (0-255).
 * @param b Blue component (0-255).
 */
void led_strip_set_color(uint8_t r, uint8_t g, uint8_t b) {
    custom_color_mode = true;
    current_r = r;
    current_g = g;
    current_b = b;
    ESP_LOGI(TAG, "Custom color set to R:%d, G:%d, B:%d", r, g, b);
    led_strip_start();  // Apply the new color immediately
}

/**
 * @brief Resets the LED strip to default RGB mode.
 */
void led_strip_reset_to_rgb(void) {
    custom_color_mode = false;
    ESP_LOGI(TAG, "LED strip reset to RGB mode");
    led_strip_start();  // Restart with default color cycling
}
