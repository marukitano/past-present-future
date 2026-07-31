#include "app_settings.h"

#define SETTINGS_PERSIST_KEY 100
#define SETTINGS_VERSION 1

#define MIN_ANIMATION_DURATION_MS 120
#define MAX_ANIMATION_DURATION_MS 2000
#define MAX_BOUNCE_DISTANCE_PX 20

static AppSettings s_settings;


static AppSettings default_settings(void) {
  return (AppSettings) {
    .version = SETTINGS_VERSION,
    .animation_effect = PPF_EFFECT_SLIDE_BOUNCE,

    .animation_duration_ms = 500,
    .bounce_distance_px = 6,

    .animate_hours = 1,
    .animate_minutes = 1
  };
}


static void validate_settings(AppSettings *settings) {
  if (settings->version != SETTINGS_VERSION) {
    *settings = default_settings();
    return;
  }

  if (settings->animation_effect > PPF_EFFECT_SLIDE_BOUNCE) {
    settings->animation_effect = PPF_EFFECT_SLIDE_BOUNCE;
  }

  if (
      settings->animation_duration_ms
          < MIN_ANIMATION_DURATION_MS
      || settings->animation_duration_ms
          > MAX_ANIMATION_DURATION_MS
  ) {
    settings->animation_duration_ms = 500;
  }

  if (
      settings->bounce_distance_px
          > MAX_BOUNCE_DISTANCE_PX
  ) {
    settings->bounce_distance_px = 6;
  }

  settings->animate_hours =
      settings->animate_hours ? 1 : 0;

  settings->animate_minutes =
      settings->animate_minutes ? 1 : 0;
}


void app_settings_load(void) {
  s_settings = default_settings();

  if (!persist_exists(SETTINGS_PERSIST_KEY)) {
    return;
  }

  AppSettings stored_settings;

  const int bytes_read = persist_read_data(
      SETTINGS_PERSIST_KEY,
      &stored_settings,
      sizeof(stored_settings)
  );

  if (bytes_read != (int)sizeof(stored_settings)) {
    return;
  }

  validate_settings(&stored_settings);
  s_settings = stored_settings;
}


const AppSettings *app_settings_get(void) {
  return &s_settings;
}


void app_settings_save(const AppSettings *settings) {
  if (!settings) {
    return;
  }

  AppSettings validated_settings = *settings;
  validate_settings(&validated_settings);

  s_settings = validated_settings;

  persist_write_data(
      SETTINGS_PERSIST_KEY,
      &s_settings,
      sizeof(s_settings)
  );
}
