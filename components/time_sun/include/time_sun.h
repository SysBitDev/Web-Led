#ifndef TIME_SUN_H
#define TIME_SUN_H

#include <time.h>
#include <stdbool.h>

void time_sun_init(void);
void time_sun_display(void);

extern time_t sunrise_time;
extern time_t sunset_time;
extern volatile bool is_night_time;

#endif // TIME_SUN_H
