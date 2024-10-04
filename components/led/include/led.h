#ifndef LED_H
#define LED_H

#include "led_strip.h"

// Define the GPIO number connected to the LED strip
#define LED_STRIP_GPIO GPIO_NUM_18  // Замініть GPIO_NUM_18 на ваш фактичний GPIO пін, якщо необхідно

// Function declarations
void led_strip_init(void);
void led_strip_start(void);
void led_strip_stop(void);
void led_strip_set_brightness(uint8_t new_brightness);
void led_strip_set_length(uint16_t length);
uint16_t led_strip_get_length(void);
void led_strip_set_color(uint8_t r, uint8_t g, uint8_t b);
void led_strip_reset_to_rgb(void);
uint8_t led_strip_get_brightness(void);
uint16_t led_strip_get_stairs_speed(void);
void led_strip_set_stairs_speed(uint16_t speed);
uint16_t led_strip_get_stairs_group_size(void);
void led_strip_set_stairs_group_size(uint16_t size);
void led_strip_get_color(uint8_t *r, uint8_t *g, uint8_t *b);
bool led_strip_get_custom_color_mode(void);
void led_strip_wave_effect(void);
void led_strip_toggle_wave_direction(void);
void led_strip_stop_effect(void); // Без 'static'
void led_strip_stairs_effect(void);
void led_strip_save_parameters(void);
void led_strip_load_parameters(void);
void led_strip_motion_effect_1(void);
void led_strip_motion_effect_2(void);
void led_strip_stairs_on(void);
void led_strip_stairs_off(void);

#endif // LED_H
