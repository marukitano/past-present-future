#pragma once

#include <pebble.h>

typedef enum {
  PPF_EFFECT_NONE = 0,
  PPF_EFFECT_SLIDE_BOUNCE = 1,
  PPF_EFFECT_ROW_WAVE = 2
} PpfAnimationEffect;

typedef enum {
  PPF_SPEED_SLOW = 0,
  PPF_SPEED_NORMAL = 1,
  PPF_SPEED_FAST = 2
} PpfAnimationSpeed;

typedef struct {
  uint8_t version;

  uint8_t animation_effect;
  uint8_t animation_speed;

  uint8_t show_date;
  uint8_t show_temperature;
} AppSettings;

typedef void (*AppSettingsChangedHandler)(void);

void app_settings_init(void);

void app_settings_deinit(void);

const AppSettings *app_settings_get(void);

void app_settings_save(
    const AppSettings *settings
);

void app_settings_set_changed_handler(
    AppSettingsChangedHandler handler
);

bool app_settings_temperature_available(void);

int16_t app_settings_temperature_c(void);

bool app_settings_request_weather(void);

uint16_t app_settings_animation_duration(
    const AppSettings *settings
);

uint8_t app_settings_bounce_distance_px(void);

uint8_t app_settings_wave_delay_percent(void);
