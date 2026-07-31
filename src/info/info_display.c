#include "info_display.h"

#include "../digit_font.h"
#include "../settings/app_settings.h"

#define INFO_Y 209

#define SMALL_DIGIT_WIDTH 16
#define SMALL_DIGIT_HEIGHT 14
#define SMALL_DIGIT_SPACING 2

#define DATE_X 7
#define DATE_GROUP_SPACING 8

#define TEMPERATURE_RIGHT_X 195
#define SYMBOL_SPACING 8

#define LETTER_C_WIDTH 16
#define MINUS_WIDTH 10

static Layer *s_info_layer;

static struct tm s_current_time;
static bool s_time_available;

static int32_t s_last_weather_slot = -1;


/*
 * Kleines C im gleichen kantigen Pixelstil.
 */
static const uint16_t LETTER_C_ROWS[
    SMALL_DIGIT_HEIGHT
] = {
  0x3FFC,
  0x7FFE,
  0xF000,
  0xE000,
  0xE000,
  0xE000,
  0xE000,
  0xE000,
  0xE000,
  0xE000,
  0xE000,
  0xF000,
  0x7FFE,
  0x3FFC
};


static bool source_pixel_is_set(
    int digit,
    int source_x,
    int source_y
) {
  if (
      digit < 0
      || digit >= PPF_DIGIT_COUNT
      || source_x < 0
      || source_x >= PPF_DIGIT_WIDTH
      || source_y < 0
      || source_y >= PPF_DIGIT_HEIGHT
  ) {
    return false;
  }

  const uint32_t row_bits =
      PPF_DIGITS[digit][source_y];

  const uint32_t mask =
      1u
      << (
        PPF_DIGIT_WIDTH
        - 1
        - source_x
      );

  return (row_bits & mask) != 0;
}


/*
 * Verkleinert eine Zeile deiner 24×21-Pixelschrift
 * auf 16×14 Pixel.
 *
 * Dabei werden keine Pebble-Systemfonts verwendet.
 */
static uint16_t scaled_digit_row(
    int digit,
    int output_y
) {
  uint16_t output_bits = 0;

  const int source_y_start =
      output_y
      * PPF_DIGIT_HEIGHT
      / SMALL_DIGIT_HEIGHT;

  const int source_y_end =
      (
        (output_y + 1)
        * PPF_DIGIT_HEIGHT
        + SMALL_DIGIT_HEIGHT
        - 1
      )
      / SMALL_DIGIT_HEIGHT;

  for (
      int output_x = 0;
      output_x < SMALL_DIGIT_WIDTH;
      ++output_x
  ) {
    const int source_x_start =
        output_x
        * PPF_DIGIT_WIDTH
        / SMALL_DIGIT_WIDTH;

    const int source_x_end =
        (
          (output_x + 1)
          * PPF_DIGIT_WIDTH
          + SMALL_DIGIT_WIDTH
          - 1
        )
        / SMALL_DIGIT_WIDTH;

    bool pixel_is_set = false;

    for (
        int source_y = source_y_start;
        source_y < source_y_end
            && !pixel_is_set;
        ++source_y
    ) {
      for (
          int source_x = source_x_start;
          source_x < source_x_end;
          ++source_x
      ) {
        if (
            source_pixel_is_set(
                digit,
                source_x,
                source_y
            )
        ) {
          pixel_is_set = true;
          break;
        }
      }
    }

    if (pixel_is_set) {
      output_bits |=
          1u
          << (
            SMALL_DIGIT_WIDTH
            - 1
            - output_x
          );
    }
  }

  return output_bits;
}


static void draw_pixel_row(
    GContext *ctx,
    uint16_t bits,
    int width,
    int origin_x,
    int origin_y
) {
  int run_start = -1;

  for (
      int column = 0;
      column <= width;
      ++column
  ) {
    const bool pixel_is_set =
        column < width
        && (
          bits
          & (
            1u
            << (
              width
              - 1
              - column
            )
          )
        );

    if (
        pixel_is_set
        && run_start < 0
    ) {
      run_start = column;
    }

    if (
        !pixel_is_set
        && run_start >= 0
    ) {
      graphics_fill_rect(
          ctx,
          GRect(
              origin_x + run_start,
              origin_y,
              column - run_start,
              1
          ),
          0,
          GCornerNone
      );

      run_start = -1;
    }
  }
}


static void draw_small_digit(
    GContext *ctx,
    int digit,
    int origin_x,
    int origin_y
) {
  for (
      int row = 0;
      row < SMALL_DIGIT_HEIGHT;
      ++row
  ) {
    draw_pixel_row(
        ctx,
        scaled_digit_row(
            digit,
            row
        ),
        SMALL_DIGIT_WIDTH,
        origin_x,
        origin_y + row
    );
  }
}


static void draw_letter_c(
    GContext *ctx,
    int origin_x,
    int origin_y
) {
  for (
      int row = 0;
      row < SMALL_DIGIT_HEIGHT;
      ++row
  ) {
    draw_pixel_row(
        ctx,
        LETTER_C_ROWS[row],
        LETTER_C_WIDTH,
        origin_x,
        origin_y + row
    );
  }
}


static void draw_minus(
    GContext *ctx,
    int origin_x,
    int origin_y
) {
  graphics_fill_rect(
      ctx,
      GRect(
          origin_x,
          origin_y + 6,
          MINUS_WIDTH,
          2
      ),
      0,
      GCornerNone
  );
}


static int number_width(
    int digit_count
) {
  if (digit_count <= 0) {
    return 0;
  }

  return (
      digit_count * SMALL_DIGIT_WIDTH
      + (
        digit_count - 1
      ) * SMALL_DIGIT_SPACING
  );
}


static int draw_number(
    GContext *ctx,
    int value,
    int digit_count,
    int origin_x,
    int origin_y
) {
  int divisor = 1;

  for (
      int index = 1;
      index < digit_count;
      ++index
  ) {
    divisor *= 10;
  }

  int x = origin_x;

  for (
      int index = 0;
      index < digit_count;
      ++index
  ) {
    const int digit =
        value / divisor % 10;

    draw_small_digit(
        ctx,
        digit,
        x,
        origin_y
    );

    x +=
        SMALL_DIGIT_WIDTH
        + SMALL_DIGIT_SPACING;

    divisor /= 10;
  }

  return x - SMALL_DIGIT_SPACING;
}


static void draw_date(
    GContext *ctx
) {
  const int day =
      s_current_time.tm_mday;

  const int month =
      s_current_time.tm_mon + 1;

  int x = DATE_X;

  x = draw_number(
      ctx,
      day,
      2,
      x,
      INFO_Y
  );

  x += DATE_GROUP_SPACING;

  draw_number(
      ctx,
      month,
      2,
      x,
      INFO_Y
  );
}


static int temperature_digit_count(
    int absolute_temperature
) {
  if (absolute_temperature >= 100) {
    return 3;
  }

  if (absolute_temperature >= 10) {
    return 2;
  }

  return 1;
}


static void draw_unavailable_temperature(
    GContext *ctx
) {
  const int width =
      MINUS_WIDTH
      + SMALL_DIGIT_SPACING
      + MINUS_WIDTH
      + SYMBOL_SPACING
      + LETTER_C_WIDTH;

  int x =
      TEMPERATURE_RIGHT_X - width;

  draw_minus(
      ctx,
      x,
      INFO_Y
  );

  x +=
      MINUS_WIDTH
      + SMALL_DIGIT_SPACING;

  draw_minus(
      ctx,
      x,
      INFO_Y
  );

  x +=
      MINUS_WIDTH
      + SYMBOL_SPACING;

  draw_letter_c(
      ctx,
      x,
      INFO_Y
  );
}


static void draw_temperature(
    GContext *ctx
) {
  if (
      !app_settings_temperature_available()
  ) {
    draw_unavailable_temperature(ctx);
    return;
  }

  const int temperature =
      app_settings_temperature_c();

  const bool is_negative =
      temperature < 0;

  const int absolute_temperature =
      is_negative
          ? -temperature
          : temperature;

  const int digit_count =
      temperature_digit_count(
          absolute_temperature
      );

  int width =
      number_width(digit_count)
      + SYMBOL_SPACING
      + LETTER_C_WIDTH;

  if (is_negative) {
    width +=
        MINUS_WIDTH
        + SMALL_DIGIT_SPACING;
  }

  int x =
      TEMPERATURE_RIGHT_X - width;

  if (is_negative) {
    draw_minus(
        ctx,
        x,
        INFO_Y
    );

    x +=
        MINUS_WIDTH
        + SMALL_DIGIT_SPACING;
  }

  x = draw_number(
      ctx,
      absolute_temperature,
      digit_count,
      x,
      INFO_Y
  );

  x += SYMBOL_SPACING;

  draw_letter_c(
      ctx,
      x,
      INFO_Y
  );
}


static void info_layer_update_proc(
    Layer *layer,
    GContext *ctx
) {
  (void)layer;

  if (!s_time_available) {
    return;
  }

  const AppSettings *settings =
      app_settings_get();

  if (!settings) {
    return;
  }

  graphics_context_set_fill_color(
      ctx,
      GColorBlack
  );

  if (settings->show_date) {
    draw_date(ctx);
  }

  if (settings->show_temperature) {
    draw_temperature(ctx);
  }
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
   * Höchstens eine Anfrage pro halber Stunde.
   */
  const int32_t weather_slot =
      (
        current_time->tm_year * 366L
        + current_time->tm_yday
      )
      * 48L
      + current_time->tm_hour * 2
      + current_time->tm_min / 30;

  if (
      weather_slot
          == s_last_weather_slot
  ) {
    return;
  }

  if (app_settings_request_weather()) {
    s_last_weather_slot =
        weather_slot;
  }
}


void info_display_update(
    const struct tm *current_time
) {
  if (!current_time) {
    return;
  }

  s_current_time = *current_time;
  s_time_available = true;

  if (s_info_layer) {
    layer_mark_dirty(s_info_layer);
  }

  maybe_request_weather(current_time);
}


static void settings_changed_handler(void) {
  time_t now = time(NULL);

  struct tm *current_time =
      localtime(&now);

  info_display_update(current_time);
}


void info_display_init(
    Layer *root_layer
) {
  if (!root_layer) {
    return;
  }

  s_info_layer = layer_create(
      GRect(
          0,
          0,
          PBL_DISPLAY_WIDTH,
          PBL_DISPLAY_HEIGHT
      )
  );

  layer_set_update_proc(
      s_info_layer,
      info_layer_update_proc
  );

  layer_add_child(
      root_layer,
      s_info_layer
  );

  app_settings_set_changed_handler(
      settings_changed_handler
  );

  settings_changed_handler();
}


void info_display_deinit(void) {
  app_settings_set_changed_handler(NULL);

  if (s_info_layer) {
    layer_destroy(s_info_layer);
    s_info_layer = NULL;
  }

  s_time_available = false;
  s_last_weather_slot = -1;
}
