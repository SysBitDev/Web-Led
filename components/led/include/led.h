#ifndef LED_H
#define LED_H

#include "sdkconfig.h"
#include <stdint.h>

/// @brief Initializes the LED strip.
void led_strip_init(void);

/// @brief Starts the LED strip with default settings.
void led_strip_start(void);

/// @brief Stops the LED strip.
void led_strip_stop(void);

/// @brief Activates the wave effect on the LED strip.
void led_strip_wave_effect(void);

/// @brief Activates the stairs effect on the LED strip.
void led_strip_stairs_effect(void);

/// @brief Stops any running effects on the LED strip.
void led_strip_stop_effect(void);

/// @brief Change the direction of the wave effect.
void led_strip_toggle_wave_direction(void);

/// @brief Sets the brightness of the LED strip.
void led_strip_set_brightness(uint8_t brightness);

/// @brief Sets the color of the LED strip.
void led_strip_set_color(uint8_t r, uint8_t g, uint8_t b);

/// @brief Resets the LED strip to the RGB mode.
void led_strip_reset_to_rgb(void);

#endif // LED_H
