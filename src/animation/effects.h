#pragma once

#include <pebble.h>

#include "../settings/app_settings.h"

int16_t ppf_effect_calculate_row_offset(
    const AppSettings *settings,
    AnimationProgress progress,
    uint8_t pixel_row,
    int16_t travel_distance
);
