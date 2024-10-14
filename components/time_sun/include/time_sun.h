#ifndef TIME_SUN_H
#define TIME_SUN_H

#include <stddef.h>
#include <time.h>

typedef struct {
    char region[64];
    char timezone[64];
    double lat;
    double lon;
} region_info_t;

void time_sun_init(void);

void get_current_time_str(char *buffer, size_t buffer_size);

time_t get_current_time_unix();

void get_current_region(char *buffer, size_t buffer_size);

void get_current_timezone(char *buffer, size_t buffer_size);

void time_sun_display(void);

void set_current_region(const char *region, const char *timezone);

void get_sun_times_json(char *buffer, size_t buffer_size);

#endif // TIME_SUN_H
