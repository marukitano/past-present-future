#include "time_row.h"

#include "../animation/effects.h"
#include "../digit_font.h"

#define DIGIT_SPACING 3
#define GROUP_SPACING 12

#define PAIR_WIDTH \
  (PPF_DIGIT_WIDTH * 2 + DIGIT_SPACING)

#define GROUP_PITCH \
  (PAIR_WIDTH + GROUP_SPACING)

#define TIME_ROW_WIDTH \
  (PAIR_WIDTH * 3 + GROUP_SPACING * 2)

#define TIME_ROW_X \
  ((PBL_DISPLAY_WIDTH - TIME_ROW_WIDTH) / 2)


static int wrap_value(
    int value,
    int modulo
) {
  value %= modulo;

  if (value < 0) {
    value += modulo;
  }

  return value;
}


static void fill_rect_clipped(
    GContext *ctx,
    GRect rect,
    const GRect *clip
) {
  if (clip) {
    grect_clip(&rect, clip);
  }

  if (
      rect.size.w <= 0
      || rect.size.h <= 0
  ) {
    return;
  }

  graphics_fill_rect(
      ctx,
      rect,
      0,
      GCornerNone
  );
}


static void draw_digit_row(
    GContext *ctx,
    int digit,
    int glyph_row,
    int origin_x,
    int origin_y,
    const GRect *clip
) {
  if (
      digit < 0
      || digit >= PPF_DIGIT_COUNT
      || glyph_row < 0
      || glyph_row >= PPF_DIGIT_HEIGHT
  ) {
    return;
  }

  const uint32_t bits =
      PPF_DIGITS[digit][glyph_row];

  int run_start = -1;

  for (
      int column = 0;
      column <= PPF_DIGIT_WIDTH;
      ++column
  ) {
    const bool pixel_is_set =
        column < PPF_DIGIT_WIDTH
        && (
          bits
          & (
            1u
            << (
              PPF_DIGIT_WIDTH
              - 1
              - column
            )
          )
        );

    if (pixel_is_set && run_start < 0) {
      run_start = column;
    }

    if (!pixel_is_set && run_start >= 0) {
      fill_rect_clipped(
          ctx,
          GRect(
              origin_x + run_start,
              origin_y + glyph_row,
              column - run_start,
              1
          ),
          clip
      );

      run_start = -1;
    }
  }
}


static void draw_two_digit_number_row(
    GContext *ctx,
    int value,
    int glyph_row,
    int origin_x,
    int origin_y,
    const GRect *clip
) {
  value = wrap_value(value, 100);

  const int tens = value / 10;
  const int ones = value % 10;

  draw_digit_row(
      ctx,
      tens,
      glyph_row,
      origin_x,
      origin_y,
      clip
  );

  draw_digit_row(
      ctx,
      ones,
      glyph_row,
      origin_x
          + PPF_DIGIT_WIDTH
          + DIGIT_SPACING,
      origin_y,
      clip
  );
}


static void draw_static_row(
    const TimeRow *row,
    GContext *ctx,
    int origin_y
) {
  for (
      int glyph_row = 0;
      glyph_row < PPF_DIGIT_HEIGHT;
      ++glyph_row
  ) {
    for (int group = 0; group < 3; ++group) {
      const int value = wrap_value(
          row->displayed_value - 1 + group,
          row->modulo
      );

      const int origin_x =
          TIME_ROW_X
          + group * GROUP_PITCH;

      draw_two_digit_number_row(
          ctx,
          value,
          glyph_row,
          origin_x,
          origin_y,
          NULL
      );
    }
  }
}


static void draw_animated_row(
    const TimeRow *row,
    GContext *ctx,
    int origin_y
) {
  const AnimationProgress progress =
      animation_controller_get_progress(
          &row->animation
      );

  /*
   * Während des Wechsels werden vier Gruppen gezeichnet:
   *
   * vorher | aktuell | nachher | übernächster
   *
   * Nach einer Verschiebung um GROUP_PITCH stehen die
   * letzten drei Gruppen exakt an den alten Positionen.
   */
  for (
      int glyph_row = 0;
      glyph_row < PPF_DIGIT_HEIGHT;
      ++glyph_row
  ) {
    const int16_t offset =
        ppf_effect_calculate_row_offset(
            row->settings,
            progress,
            (uint8_t)glyph_row,
            GROUP_PITCH
        );

    for (int group = 0; group < 4; ++group) {
      const int value = wrap_value(
          row->displayed_value - 1 + group,
          row->modulo
      );

      const int origin_x =
          TIME_ROW_X
          + group * GROUP_PITCH
          - offset;

      draw_two_digit_number_row(
          ctx,
          value,
          glyph_row,
          origin_x,
          origin_y,
          NULL
      );
    }
  }
}


static void row_animation_stopped(
    bool finished,
    void *context
) {
  TimeRow *row = context;

  if (!row) {
    return;
  }

  row->animating = false;

  if (finished) {
    row->displayed_value =
        row->target_value;
  }

  if (row->target_layer) {
    layer_mark_dirty(row->target_layer);
  }
}


void time_row_init(
    TimeRow *row,
    Layer *target_layer,
    int modulo,
    const AppSettings *settings
) {
  if (!row) {
    return;
  }

  *row = (TimeRow) {
    .displayed_value = 0,
    .target_value = 0,
    .modulo = modulo,

    .initialized = false,
    .animating = false,

    .target_layer = target_layer,
    .settings = settings
  };

  animation_controller_init(
      &row->animation,
      target_layer,
      row_animation_stopped,
      row
  );
}


void time_row_deinit(TimeRow *row) {
  if (!row) {
    return;
  }

  animation_controller_deinit(
      &row->animation
  );

  row->target_layer = NULL;
  row->settings = NULL;
}


void time_row_set_value(
    TimeRow *row,
    int value,
    bool animate
) {
  if (!row || row->modulo <= 0) {
    return;
  }

  value = wrap_value(value, row->modulo);

  if (!row->initialized) {
    row->displayed_value = value;
    row->target_value = value;
    row->initialized = true;

    if (row->target_layer) {
      layer_mark_dirty(row->target_layer);
    }

    return;
  }

  if (
      animation_controller_is_running(
          &row->animation
      )
  ) {
    animation_controller_cancel(
        &row->animation
    );

    row->displayed_value =
        row->target_value;

    row->animating = false;
  }

  if (value == row->displayed_value) {
    row->target_value = value;
    return;
  }

  const int forward_distance =
      wrap_value(
          value - row->displayed_value,
          row->modulo
      );

  row->target_value = value;

  const bool can_animate =
      animate
      && row->settings
      && row->settings->animation_effect
          != PPF_EFFECT_NONE
      && forward_distance == 1;

  if (!can_animate) {
    row->displayed_value = value;
    row->animating = false;

    if (row->target_layer) {
      layer_mark_dirty(row->target_layer);
    }

    return;
  }

  row->animating = true;

  if (
      !animation_controller_start(
          &row->animation,
          row->settings->animation_duration_ms
      )
  ) {
    row->displayed_value = value;
    row->animating = false;

    if (row->target_layer) {
      layer_mark_dirty(row->target_layer);
    }
  }
}


void time_row_draw(
    const TimeRow *row,
    GContext *ctx,
    int origin_y
) {
  if (!row || !row->initialized) {
    return;
  }

  if (
      row->animating
      && animation_controller_is_running(
          &row->animation
      )
  ) {
    draw_animated_row(
        row,
        ctx,
        origin_y
    );

    return;
  }

  draw_static_row(
      row,
      ctx,
      origin_y
  );
}


void time_row_draw_present_overlay(
    const TimeRow *row,
    GContext *ctx,
    int origin_y
) {
  if (
      !row
      || !row->initialized
  ) {
    return;
  }

  /*
   * Der Korridor wird automatisch an die in den
   * Einstellungen gewählte Bounce-Stärke angepasst.
   */
  const int16_t bounce_distance =
      row->settings
          ? row->settings->bounce_distance_px
          : 0;

  /*
   * Die Zahl soll gepunktet aus dem FUTURE-Bereich
   * herausfahren und erst im PRESENT-Bereich massiv werden.
   *
   * Links brauchen wir Platz für den Bounce.
   * Rechts wird der maskenfreie Bereich nur bis kurz vor
   * die FUTURE-Startposition erweitert.
   */
  const int16_t left_clearance =
      bounce_distance + 2;

  const int16_t right_clearance =
      GROUP_SPACING - 2;

  const GRect clear_corridor = GRect(
      TIME_ROW_X
          + GROUP_PITCH
          - left_clearance,
      origin_y,
      PAIR_WIDTH
          + left_clearance
          + right_clearance,
      PPF_DIGIT_HEIGHT
  );

  const bool is_animating =
      row->animating
      && animation_controller_is_running(
          &row->animation
      );

  const AnimationProgress progress =
      is_animating
          ? animation_controller_get_progress(
              &row->animation
            )
          : ANIMATION_NORMALIZED_MAX;

  for (
      int glyph_row = 0;
      glyph_row < PPF_DIGIT_HEIGHT;
      ++glyph_row
  ) {
    int value = row->displayed_value;

    int origin_x =
        TIME_ROW_X
        + GROUP_PITCH;

    if (is_animating) {
      const int16_t offset =
          ppf_effect_calculate_row_offset(
              row->settings,
              progress,
              (uint8_t)glyph_row,
              GROUP_PITCH
          );

      /*
       * Die neue PRESENT-Zahl ist während der Animation
       * die dritte Gruppe der vier gezeichneten Gruppen.
       */
      origin_x =
          TIME_ROW_X
          + 2 * GROUP_PITCH
          - offset;

      value = row->target_value;
    }

    draw_two_digit_number_row(
        ctx,
        value,
        glyph_row,
        origin_x,
        origin_y,
        &clear_corridor
    );
  }
}
