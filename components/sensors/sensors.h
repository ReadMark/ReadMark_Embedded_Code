#ifndef SENSORS_H
#define SENSORS_H

#include <stdbool.h>

void sensors_init(void);
int read_touch_sensor(void);
bool is_touch_pressed(void);
int read_pressure_sensor(void);
bool is_book_closed(void);
void sleep_mode(void);

#endif