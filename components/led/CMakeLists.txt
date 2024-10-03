idf_component_register(
    SRCS "led.c"
    INCLUDE_DIRS "include"
    REQUIRES driver nvs_flash led_strip esp_timer
)
