#include "info_display.h"

#include "../digit_font.h"
#include "../settings/app_settings.h"
#include "../symbol_font.h"

#define INFO_Y 209

#define SMALL_GLYPH_WIDTH 16
#define SMALL_GLYPH_HEIGHT 14

#define SMALL_DIGIT_SPACING 2

#define DATE_X 7
#define DATE_GROUP_SPACING 8

#define TEMPERATURE_RIGHT_X 195

/*
 * Das Gradzeichen sitzt rechts in seinem
 * 24-Pixel-Quellfeld.
 *
 * Durch die Überlappung erscheint es mit etwa
 * vier sichtbaren Pixeln Abstand zur Temperatur.
 */
#define DEGREE_CELL_OVERLAP 8

/*
 * Sichtbarer Abstand zwischen Gradzeichen und C.
 */
#define DEGREE_C_SPACING 3

#define MINUS_WIDTH 10

static Layer *s_info_layer;

static struct tm s_current_time;
static bool s_time_available;

static int32_t s_last_weather_slot = -1;


static uint16_t scaled_glyph_row(
    const uint32_t *source_rows,
    int source_width,
    int source_height,
    int output_y
) {
  uint16_t output_bits = 0;

  const int source_y_start =
      output_y
      * source_height
      / SMALL_GLYPH_HEIGHT;

  const int source_y_end =
      (
        (output_y + 1)
        * source_height
        + SMALL_GLYPH_HEIGHT
        - 1
      )
      / SMALL_GLYPH_HEIGHT;

  for (
      int output_x = 0;
      output_x < SMALL_GLYPH_WIDTH;
      ++output_x
  ) {
    const int source_x_start =
        output_x
        * source_width
        / SMALL_GLYPH_WIDTH;

    const int source_x_end =
        (
          (output_x + 1)
          * source_width
          + SMALL_GLYPH_WIDTH
          - 1
        )
        / SMALL_GLYPH_WIDTH;

    bool pixel_is_set = false;

    for (
        int source_y = source_y_start;
        source_y < source_y_end
            && !pixel_is_set;
        ++source_y
    ) {
      const uint32_t source_bits =
          source_rows[source_y];

      for (
          int source_x = source_x_start;
          source_x < source_x_end;
          ++source_x
      ) {
        const uint32_t mask =
            1u
            << (
              source_width
              - 1
              - source_x
            );

        if (source_bits & mask) {
          pixel_is_set = true;
          break;
        }
      }
    }

    if (pixel_is_set) {
      output_bits |=
          1u
          << (
            SMALL_GLYPH_WIDTH
            - 1
            - output_x
          );
    }
  }

  return output_bits;
}


/*
 * Nearest-Neighbour-Skalierung für sehr kleine,
 * hohle Symbole.
 *
 * Die normale Flächenskalierung ist für die Ziffern
 * gut, würde aber das Loch im Gradzeichen auffüllen.
 */
static uint16_t scaled_glyph_row_nearest(
    const uint32_t *source_rows,
    int source_width,
    int source_height,
    int output_y
) {
  uint16_t output_bits = 0;

  const int source_y =
      (
        (output_y * 2 + 1)
        * source_height
      )
      / (
        SMALL_GLYPH_HEIGHT * 2
      );

  for (
      int output_x = 0;
      output_x < SMALL_GLYPH_WIDTH;
      ++output_x
  ) {
    const int source_x =
        (
          (output_x * 2 + 1)
          * source_width
        )
        / (
          SMALL_GLYPH_WIDTH * 2
        );

    const uint32_t mask =
        1u
        << (
          source_width
          - 1
          - source_x
        );

    if (source_rows[source_y] & mask) {
      output_bits |=
          1u
          << (
            SMALL_GLYPH_WIDTH
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


static void draw_small_glyph(
    GContext *ctx,
    const uint32_t *source_rows,
    int source_width,
    int source_height,
    int origin_x,
    int origin_y
) {
  for (
      int row = 0;
      row < SMALL_GLYPH_HEIGHT;
      ++row
  ) {
    draw_pixel_row(
        ctx,
        scaled_glyph_row(
            source_rows,
            source_width,
            source_height,
            row
        ),
        SMALL_GLYPH_WIDTH,
        origin_x,
        origin_y + row
    );
  }
}


static void draw_small_digit(
    GContext *ctx,
    int digit,
    int origin_x,
    int origin_y
) {
  if (
      digit < 0
      || digit >= PPF_DIGIT_COUNT
  ) {
    return;
  }

  draw_small_glyph(
      ctx,
      PPF_DIGITS[digit],
      PPF_DIGIT_WIDTH,
      PPF_DIGIT_HEIGHT,
      origin_x,
      origin_y
  );
}


static void draw_small_symbol(
    GContext *ctx,
    PpfSymbol symbol,
    int origin_x,
    int origin_y
) {
  if (
      (unsigned int)symbol
          >= PPF_SYMBOL_COUNT
  ) {
    return;
  }

  /*
   * Das kleine Gradzeichen braucht eine punktgenaue
   * Skalierung, damit sein transparentes Loch erhalten
   * bleibt.
   */
  if (symbol == PPF_SYMBOL_DEGREE) {
    for (
        int row = 0;
        row < SMALL_GLYPH_HEIGHT;
        ++row
    ) {
      draw_pixel_row(
          ctx,
          scaled_glyph_row_nearest(
              PPF_SYMBOLS[symbol],
              PPF_SYMBOL_WIDTH,
              PPF_SYMBOL_HEIGHT,
              row
          ),
          SMALL_GLYPH_WIDTH,
          origin_x,
          origin_y + row
      );
    }

    return;
  }

  draw_small_glyph(
      ctx,
      PPF_SYMBOLS[symbol],
      PPF_SYMBOL_WIDTH,
      PPF_SYMBOL_HEIGHT,
      origin_x,
      origin_y
  );
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
      digit_count * SMALL_GLYPH_WIDTH
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
        SMALL_GLYPH_WIDTH
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


/*
 * Zeichnet °C direkt hinter die Temperatur.
 *
 * Das Gradzeichen wird zuerst gezeichnet,
 * anschließend das C.
 */
static int draw_celsius(
    GContext *ctx,
    int number_end_x,
    int origin_y
) {
  const int degree_x =
      number_end_x
      - DEGREE_CELL_OVERLAP;

  draw_small_symbol(
      ctx,
      PPF_SYMBOL_DEGREE,
      degree_x,
      origin_y
  );

  const int c_x =
      degree_x
      + SMALL_GLYPH_WIDTH
      + DEGREE_C_SPACING;

  draw_small_symbol(
      ctx,
      PPF_SYMBOL_C,
      c_x,
      origin_y
  );

  return c_x + SMALL_GLYPH_WIDTH;
}


static int celsius_advance_width(void) {
  return (
      SMALL_GLYPH_WIDTH * 2
      - DEGREE_CELL_OVERLAP
      + DEGREE_C_SPACING
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
      + celsius_advance_width();

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

  x += MINUS_WIDTH;

  draw_celsius(
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
      + celsius_advance_width();

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

  draw_celsius(
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
