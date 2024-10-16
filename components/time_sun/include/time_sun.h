#ifndef TIME_SUN_H
#define TIME_SUN_H

#include <time.h>
#include <stdbool.h>

extern time_t sunrise_time;
extern time_t sunset_time;
extern volatile bool is_night_time;
extern volatile bool ignore_sun;

void time_sun_init(void);
void time_sun_display(void);

#endif // TIME_SUN_H
