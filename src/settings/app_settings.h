#pragma once

#include <pebble.h>

typedef enum {
  PPF_EFFECT_NONE = 0,
  PPF_EFFECT_SLIDE_BOUNCE = 1
} PpfAnimationEffect;

typedef struct {
  uint8_t version;
  uint8_t animation_effect;

  uint16_t animation_duration_ms;
  uint8_t bounce_distance_px;

  uint8_t animate_hours;
  uint8_t animate_minutes;
} AppSettings;

void app_settings_load(void);

const AppSettings *app_settings_get(void);

void app_settings_save(const AppSettings *settings);
