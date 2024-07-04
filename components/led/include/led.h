#ifndef LED_H
#define LED_H

#include "sdkconfig.h"
#include <stdint.h>

/**
 * @brief Initializes the LED strip.
 * Sets up the RMT channel and configures the LED strip encoder based on predefined settings.
 */
void led_strip_init(void);

/**
 * @brief Starts the LED strip with default settings.
 * Begins LED operation by applying the current configuration for color and brightness.
 */
void led_strip_start(void);

/**
 * @brief Stops the LED strip.
 * Turns off all LEDs and stops any ongoing RMT transmission.
 */
void led_strip_stop(void);

/**
 * @brief Activates the wave effect on the LED strip.
 * Launches a task that creates a moving wave pattern across the LED strip.
 */
void led_strip_wave_effect(void);

/**
 * @brief Activates the stairs effect on the LED strip.
 * Starts a repeating pattern that simulates lights turning on and off in a sequence resembling stairs.
 */
void led_strip_stairs_effect(void);

/**
 * @brief Stops any running effects on the LED strip.
 * Terminates any tasks controlling LED animations and clears the strip to its off state.
 */
void led_strip_stop_effect(void);

/**
 * @brief Changes the direction of the wave effect.
 * Toggles the movement direction of the wave pattern between forward and backward.
 */
void led_strip_toggle_wave_direction(void);

/**
 * @brief Sets the brightness of the LED strip.
 * @param brightness New brightness level (0-100), where 0 is completely off and 100 is the brightest setting.
 */
void led_strip_set_brightness(uint8_t brightness);

/**
 * @brief Sets the color of the LED strip.
 * Changes the color configuration to a specific RGB value.
 * @param r Red component of the color (0-255).
 * @param g Green component of the color (0-255).
 * @param b Blue component of the color (0-255).
 */
void led_strip_set_color(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Resets the LED strip to the RGB mode.
 * Switches the LED strip back to default color cycling mode from any custom settings.
 */
void led_strip_reset_to_rgb(void);

/**
 * @brief Activates a predefined motion effect 1 on the LED strip.
 * This function starts a specific pattern or animation that is preset as "motion effect 1."
 */
void led_strip_motion_effect_1(void);

/**
 * @brief Activates a predefined motion effect 2 on the LED strip.
 * This function initiates a distinct pattern or animation predefined as "motion effect 2."
 */
void led_strip_motion_effect_2(void);

#endif // LED_H
