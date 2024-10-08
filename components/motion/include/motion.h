#ifndef MOTION_H
#define MOTION_H

#include <stdint.h>

void motion_init(void);
void motion_start(void);
void motion_stop(void);

uint32_t motion_get_delay(void);
void motion_set_delay(uint32_t delay_ms);

#endif // MOTION_H
