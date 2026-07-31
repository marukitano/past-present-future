#include "app_settings.h"

#define SETTINGS_PERSIST_KEY 100
#define TEMPERATURE_PERSIST_KEY 101

#define SETTINGS_VERSION 4

#define DEFAULT_BOUNCE_DISTANCE_PX 6
#define DEFAULT_WAVE_DELAY_PERCENT 26

static AppSettings s_settings;

static int16_t s_temperature_c;
static bool s_temperature_available;

static AppSettingsChangedHandler
    s_changed_handler;


static AppSettings default_settings(void) {
  return (AppSettings) {
    .version = SETTINGS_VERSION,

    .animation_effect =
        PPF_EFFECT_ROW_WAVE,

    .animation_speed =
        PPF_SPEED_NORMAL,

    .show_date = 1,
    .show_temperature = 1
  };
}


static void notify_changed(void) {
  if (s_changed_handler) {
    s_changed_handler();
  }
}


static void validate_settings(
    AppSettings *settings
) {
  if (!settings) {
    return;
  }

  if (settings->version != SETTINGS_VERSION) {
    *settings = default_settings();
    return;
  }

  if (
      settings->animation_effect
          > PPF_EFFECT_ROW_WAVE
  ) {
    settings->animation_effect =
        PPF_EFFECT_ROW_WAVE;
  }

  if (
      settings->animation_speed
          > PPF_SPEED_FAST
  ) {
    settings->animation_speed =
        PPF_SPEED_NORMAL;
  }

  settings->show_date =
      settings->show_date ? 1 : 0;

  settings->show_temperature =
      settings->show_temperature ? 1 : 0;
}


static void load_settings(void) {
  s_settings = default_settings();

  if (persist_exists(SETTINGS_PERSIST_KEY)) {
    AppSettings stored_settings;

    const int bytes_read = persist_read_data(
        SETTINGS_PERSIST_KEY,
        &stored_settings,
        sizeof(stored_settings)
    );

    if (
        bytes_read
            == (int)sizeof(stored_settings)
    ) {
      validate_settings(&stored_settings);
      s_settings = stored_settings;
    }
  }

  s_temperature_available = false;
  s_temperature_c = 0;

  if (
      persist_exists(
          TEMPERATURE_PERSIST_KEY
      )
  ) {
    s_temperature_c = (int16_t)persist_read_int(
        TEMPERATURE_PERSIST_KEY
    );

    s_temperature_available = true;
  }
}


void app_settings_save(
    const AppSettings *settings
) {
  if (!settings) {
    return;
  }

  AppSettings validated = *settings;

  validate_settings(&validated);
  s_settings = validated;

  persist_write_data(
      SETTINGS_PERSIST_KEY,
      &s_settings,
      sizeof(s_settings)
  );

  notify_changed();
}


static void inbox_received_handler(
    DictionaryIterator *iterator,
    void *context
) {
  (void)context;

  AppSettings updated = s_settings;
  bool received_settings = false;

  Tuple *effect_tuple = dict_find(
      iterator,
      MESSAGE_KEY_AnimationEffect
  );

  if (effect_tuple) {
    updated.animation_effect =
        (uint8_t)effect_tuple->value->int32;

    received_settings = true;
  }

  Tuple *speed_tuple = dict_find(
      iterator,
      MESSAGE_KEY_AnimationSpeed
  );

  if (speed_tuple) {
    updated.animation_speed =
        (uint8_t)speed_tuple->value->int32;

    received_settings = true;
  }

  Tuple *date_tuple = dict_find(
      iterator,
      MESSAGE_KEY_ShowDate
  );

  if (date_tuple) {
    updated.show_date =
        date_tuple->value->int32 ? 1 : 0;

    received_settings = true;
  }

  Tuple *temperature_enabled_tuple =
      dict_find(
          iterator,
          MESSAGE_KEY_ShowTemperature
      );

  if (temperature_enabled_tuple) {
    updated.show_temperature =
        temperature_enabled_tuple
            ->value->int32
        ? 1
        : 0;

    received_settings = true;
  }

  if (received_settings) {
    app_settings_save(&updated);

    APP_LOG(
        APP_LOG_LEVEL_INFO,
        "Settings: effect=%u speed=%u date=%u temp=%u",
        (unsigned int)s_settings.animation_effect,
        (unsigned int)s_settings.animation_speed,
        (unsigned int)s_settings.show_date,
        (unsigned int)s_settings.show_temperature
    );
  }

  Tuple *temperature_tuple = dict_find(
      iterator,
      MESSAGE_KEY_Temperature
  );

  if (temperature_tuple) {
    const int32_t temperature =
        temperature_tuple->value->int32;

    if (
        temperature >= -100
        && temperature <= 100
    ) {
      s_temperature_c =
          (int16_t)temperature;

      s_temperature_available = true;

      persist_write_int(
          TEMPERATURE_PERSIST_KEY,
          s_temperature_c
      );

      APP_LOG(
          APP_LOG_LEVEL_INFO,
          "Temperature: %d C",
          (int)s_temperature_c
      );

      notify_changed();
    }
  }
}


static void inbox_dropped_handler(
    AppMessageResult reason,
    void *context
) {
  (void)context;

  APP_LOG(
      APP_LOG_LEVEL_ERROR,
      "Inbox dropped: %d",
      (int)reason
  );
}


void app_settings_init(void) {
  load_settings();

  app_message_register_inbox_received(
      inbox_received_handler
  );

  app_message_register_inbox_dropped(
      inbox_dropped_handler
  );

  const AppMessageResult result =
      app_message_open(128, 64);

  if (result != APP_MSG_OK) {
    APP_LOG(
        APP_LOG_LEVEL_ERROR,
        "AppMessage initialization failed: %d",
        (int)result
    );
  }
}


void app_settings_deinit(void) {
  s_changed_handler = NULL;
  app_message_deregister_callbacks();
}


const AppSettings *app_settings_get(void) {
  return &s_settings;
}


void app_settings_set_changed_handler(
    AppSettingsChangedHandler handler
) {
  s_changed_handler = handler;
}


bool app_settings_temperature_available(void) {
  return s_temperature_available;
}


int16_t app_settings_temperature_c(void) {
  return s_temperature_c;
}


bool app_settings_request_weather(void) {
  if (!s_settings.show_temperature) {
    return false;
  }

  DictionaryIterator *iterator = NULL;

  const AppMessageResult begin_result =
      app_message_outbox_begin(&iterator);

  if (
      begin_result != APP_MSG_OK
      || !iterator
  ) {
    return false;
  }

  dict_write_uint8(
      iterator,
      MESSAGE_KEY_WeatherRequest,
      1
  );

  const AppMessageResult send_result =
      app_message_outbox_send();

  return send_result == APP_MSG_OK;
}


uint16_t app_settings_animation_duration(
    const AppSettings *settings
) {
  if (!settings) {
    return 500;
  }

  switch (
      (PpfAnimationSpeed)
          settings->animation_speed
  ) {
    case PPF_SPEED_SLOW:
      return 700;

    case PPF_SPEED_FAST:
      return 320;

    case PPF_SPEED_NORMAL:
    default:
      return 500;
  }
}


uint8_t app_settings_bounce_distance_px(void) {
  return DEFAULT_BOUNCE_DISTANCE_PX;
}


uint8_t app_settings_wave_delay_percent(void) {
  return DEFAULT_WAVE_DELAY_PERCENT;
}
