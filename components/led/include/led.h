#ifndef LED_H
#define LED_H

#include "led_strip.h"
#include "esp_err.h"

#define LED_STRIP_GPIO GPIO_NUM_25

void led_strip_init(void);
void led_strip_start(void);
void led_strip_stop(void);
void led_strip_set_brightness(uint8_t new_brightness);
uint8_t led_strip_get_brightness(void);

esp_err_t led_strip_set_length(uint16_t length);
uint16_t led_strip_get_length(void);

bool led_strip_is_effect_running(void);

void led_strip_set_color(uint8_t r, uint8_t g, uint8_t b);
void led_strip_get_color(uint8_t *r, uint8_t *g, uint8_t *b);
void led_strip_reset_to_rgb(void);

void led_strip_set_rgb_mode(bool enable);

void hsv_2_rgb(float h, float s, float v, uint8_t *r, uint8_t *g, uint8_t *b);

void led_strip_set_stairs_speed(uint16_t speed);
uint16_t led_strip_get_stairs_speed(void);

void led_strip_set_stairs_group_size(uint16_t size);
uint16_t led_strip_get_stairs_group_size(void);

void led_strip_wave_effect(void);
void led_strip_toggle_wave_direction(void);

void led_strip_stairs_effect(void);
void led_strip_stairs_effect_from_start(void);
void led_strip_stairs_effect_from_end(void);
void led_strip_stairs_effect_both(void);

void led_strip_stop_effect(void);
bool led_strip_get_custom_color_mode(void);

bool led_strip_get_rgb_mode(void);

void led_strip_save_parameters(void);
void led_strip_load_parameters(void);

#endif // LED_H