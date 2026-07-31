#pragma once

#include <pebble.h>

void info_display_init(Layer *root_layer);

void info_display_deinit(void);

void info_display_update(
    const struct tm *current_time
);
