#include "info_display.h"

#include "../settings/app_settings.h"

#define INFO_ROW_Y 198
#define INFO_ROW_HEIGHT 24

#define DATE_X 6
#define DATE_WIDTH 88

#define TEMPERATURE_X 106
#define TEMPERATURE_WIDTH 88

static TextLayer *s_date_layer;
static TextLayer *s_temperature_layer;

static char s_date_text[16];
static char s_temperature_text[12];

static int32_t s_last_weather_slot = -1;


static TextLayer *create_info_layer(
    Layer *root_layer,
    GRect frame,
    GTextAlignment alignment
) {
  TextLayer *text_layer =
      text_layer_create(frame);

  text_layer_set_background_color(
      text_layer,
      GColorClear
  );

  text_layer_set_text_color(
      text_layer,
      GColorBlack
  );

  text_layer_set_font(
      text_layer,
      fonts_get_system_font(
          FONT_KEY_GOTHIC_18
      )
  );

  text_layer_set_text_alignment(
      text_layer,
      alignment
  );

  layer_add_child(
      root_layer,
      text_layer_get_layer(text_layer)
  );

  return text_layer;
}


static void maybe_request_weather(
    const struct tm *current_time
) {
  const AppSettings *settings =
      app_settings_get();

  if (
      !settings
      || !settings->show_temperature
      || !current_time
  ) {
    return;
  }

  /*
   * Eine Aktualisierung pro halber Stunde.
   * Funktioniert auch im Sekunden-Demomodus, ohne
   * innerhalb einer Minute mehrfach anzufragen.
   */
  const int32_t slot =
      (
        current_time->tm_year * 366L
        + current_time->tm_yday
      )
      * 48L
      + current_time->tm_hour * 2
      + current_time->tm_min / 30;

  if (slot == s_last_weather_slot) {
    return;
  }

  if (app_settings_request_weather()) {
    s_last_weather_slot = slot;
  }
}


void info_display_update(
    const struct tm *current_time
) {
  if (
      !current_time
      || !s_date_layer
      || !s_temperature_layer
  ) {
    return;
  }

  const AppSettings *settings =
      app_settings_get();

  if (!settings) {
    return;
  }

  Layer *date_layer =
      text_layer_get_layer(s_date_layer);

  Layer *temperature_layer =
      text_layer_get_layer(
          s_temperature_layer
      );

  layer_set_hidden(
      date_layer,
      !settings->show_date
  );

  layer_set_hidden(
      temperature_layer,
      !settings->show_temperature
  );

  if (settings->show_date) {
    strftime(
        s_date_text,
        sizeof(s_date_text),
        "%a %d",
        current_time
    );

    text_layer_set_text(
        s_date_layer,
        s_date_text
    );
  }

  if (settings->show_temperature) {
    if (
        app_settings_temperature_available()
    ) {
      snprintf(
          s_temperature_text,
          sizeof(s_temperature_text),
          "%d°C",
          (int)app_settings_temperature_c()
      );
    } else {
      snprintf(
          s_temperature_text,
          sizeof(s_temperature_text),
          "--°C"
      );
    }

    text_layer_set_text(
        s_temperature_layer,
        s_temperature_text
    );
  }

  maybe_request_weather(current_time);
}


static void settings_changed_handler(void) {
  time_t now = time(NULL);

  struct tm *current_time =
      localtime(&now);

  info_display_update(current_time);
}


void info_display_init(Layer *root_layer) {
  if (!root_layer) {
    return;
  }

  s_date_layer = create_info_layer(
      root_layer,
      GRect(
          DATE_X,
          INFO_ROW_Y,
          DATE_WIDTH,
          INFO_ROW_HEIGHT
      ),
      GTextAlignmentLeft
  );

  s_temperature_layer = create_info_layer(
      root_layer,
      GRect(
          TEMPERATURE_X,
          INFO_ROW_Y,
          TEMPERATURE_WIDTH,
          INFO_ROW_HEIGHT
      ),
      GTextAlignmentRight
  );

  app_settings_set_changed_handler(
      settings_changed_handler
  );

  settings_changed_handler();
}


void info_display_deinit(void) {
  app_settings_set_changed_handler(NULL);

  text_layer_destroy(s_date_layer);
  text_layer_destroy(s_temperature_layer);

  s_date_layer = NULL;
  s_temperature_layer = NULL;

  s_last_weather_slot = -1;
}
