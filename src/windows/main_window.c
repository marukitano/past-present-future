#include <pebble.h>

#include "../digit_font.h"
#include "main_window.h"

#if !defined(PBL_PLATFORM_EMERY)
#error "Past Present Future supports only Pebble Time 2 / Emery."
#endif

#define DIGIT_SPACING 1
#define GROUP_SPACING 12

#define PAIR_WIDTH \
  (PPF_DIGIT_WIDTH * 2 + DIGIT_SPACING)

#define TIME_ROW_WIDTH \
  (PAIR_WIDTH * 3 + GROUP_SPACING * 2)

#define TIME_ROW_X \
  ((PBL_DISPLAY_WIDTH - TIME_ROW_WIDTH) / 2)

#define HOUR_ROW_Y 18
#define MINUTE_ROW_Y 45

/*
 * Die ursprünglichen 144 × 168 Grafiken werden für das
 * 200 × 228 Display der Pebble Time 2 pixelgenau skaliert.
 */
#define COLUMN_WIDTH 46
#define COLUMN_HEIGHT PBL_DISPLAY_HEIGHT
#define COLUMN_Y 0

#define PAST_COLUMN_X (-4)
#define PRESENT_COLUMN_X 76
#define FUTURE_COLUMN_X 156

static Window *s_window;
static Layer *s_time_layer;

static BitmapLayer *s_past_bitmap_layer;
static BitmapLayer *s_present_bitmap_layer;
static BitmapLayer *s_future_bitmap_layer;
static BitmapLayer *s_mask_bitmap_layer;

static GBitmap *s_past_gbitmap;
static GBitmap *s_present_gbitmap;
static GBitmap *s_future_gbitmap;
static GBitmap *s_mask_gbitmap;

static int s_hours;
static int s_minutes;


static void draw_digit(
    GContext *ctx,
    int digit,
    int origin_x,
    int origin_y
) {
  if (digit < 0 || digit >= PPF_DIGIT_COUNT) {
    return;
  }

  for (int row = 0; row < PPF_DIGIT_HEIGHT; ++row) {
    const uint32_t bits = PPF_DIGITS[digit][row];
    int run_start = -1;

    /*
     * Statt jeden Pixel einzeln zu zeichnen, werden
     * zusammenhängende schwarze Pixel als Rechteck gezeichnet.
     */
    for (int column = 0; column <= PPF_DIGIT_WIDTH; ++column) {
      const bool pixel_is_set =
          column < PPF_DIGIT_WIDTH
          && (bits & (1u << (PPF_DIGIT_WIDTH - 1 - column)));

      if (pixel_is_set && run_start < 0) {
        run_start = column;
      }

      if (!pixel_is_set && run_start >= 0) {
        graphics_fill_rect(
            ctx,
            GRect(
                origin_x + run_start,
                origin_y + row,
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
}


static void draw_two_digit_number(
    GContext *ctx,
    int value,
    int origin_x,
    int origin_y
) {
  const int tens = value / 10;
  const int ones = value % 10;

  draw_digit(
      ctx,
      tens,
      origin_x,
      origin_y
  );

  draw_digit(
      ctx,
      ones,
      origin_x + PPF_DIGIT_WIDTH + DIGIT_SPACING,
      origin_y
  );
}


static void draw_past_present_future(
    GContext *ctx,
    int current_value,
    int modulo,
    int origin_y
) {
  const int values[3] = {
      (current_value + modulo - 1) % modulo,
      current_value,
      (current_value + 1) % modulo
  };

  for (int group = 0; group < 3; ++group) {
    const int origin_x =
        TIME_ROW_X
        + group * (PAIR_WIDTH + GROUP_SPACING);

    draw_two_digit_number(
        ctx,
        values[group],
        origin_x,
        origin_y
    );
  }
}


static void time_layer_update_proc(
    Layer *layer,
    GContext *ctx
) {
  (void)layer;

  graphics_context_set_fill_color(ctx, GColorBlack);

  draw_past_present_future(
      ctx,
      s_hours,
      24,
      HOUR_ROW_Y
  );

  draw_past_present_future(
      ctx,
      s_minutes,
      60,
      MINUTE_ROW_Y
  );
}


static void tick_handler(
    struct tm *tick_time,
    TimeUnits units_changed
) {
  (void)units_changed;

  s_hours = tick_time->tm_hour;
  s_minutes = tick_time->tm_min;

  if (s_time_layer) {
    layer_mark_dirty(s_time_layer);
  }
}


static void create_column_layers(Layer *root_layer) {
  s_past_gbitmap =
      gbitmap_create_with_resource(RESOURCE_ID_PAST);

  s_present_gbitmap =
      gbitmap_create_with_resource(RESOURCE_ID_PRESENT);

  s_future_gbitmap =
      gbitmap_create_with_resource(RESOURCE_ID_FUTURE);

  s_past_bitmap_layer = bitmap_layer_create(
      GRect(
          PAST_COLUMN_X,
          COLUMN_Y,
          COLUMN_WIDTH,
          COLUMN_HEIGHT
      )
  );

  s_present_bitmap_layer = bitmap_layer_create(
      GRect(
          PRESENT_COLUMN_X,
          COLUMN_Y,
          COLUMN_WIDTH,
          COLUMN_HEIGHT
      )
  );

  s_future_bitmap_layer = bitmap_layer_create(
      GRect(
          FUTURE_COLUMN_X,
          COLUMN_Y,
          COLUMN_WIDTH,
          COLUMN_HEIGHT
      )
  );

  bitmap_layer_set_bitmap(
      s_past_bitmap_layer,
      s_past_gbitmap
  );

  bitmap_layer_set_bitmap(
      s_present_bitmap_layer,
      s_present_gbitmap
  );

  bitmap_layer_set_bitmap(
      s_future_bitmap_layer,
      s_future_gbitmap
  );

  layer_add_child(
      root_layer,
      bitmap_layer_get_layer(s_past_bitmap_layer)
  );

  layer_add_child(
      root_layer,
      bitmap_layer_get_layer(s_present_bitmap_layer)
  );

  layer_add_child(
      root_layer,
      bitmap_layer_get_layer(s_future_bitmap_layer)
  );
}


static void window_load(Window *window) {
  Layer *root_layer = window_get_root_layer(window);

  create_column_layers(root_layer);

  s_time_layer = layer_create(
      GRect(
          0,
          0,
          PBL_DISPLAY_WIDTH,
          PBL_DISPLAY_HEIGHT
      )
  );

  layer_set_update_proc(
      s_time_layer,
      time_layer_update_proc
  );

  layer_add_child(
      root_layer,
      s_time_layer
  );

  /*
   * Die Maske liegt über den Zahlen. Dadurch ist PRESENT
   * vollständig sichtbar, während PAST und FUTURE durch
   * das charakteristische Punktraster erscheinen.
   */
  s_mask_gbitmap =
      gbitmap_create_with_resource(RESOURCE_ID_MASK);

  s_mask_bitmap_layer = bitmap_layer_create(
      GRect(
          0,
          0,
          PBL_DISPLAY_WIDTH,
          PBL_DISPLAY_HEIGHT
      )
  );

  bitmap_layer_set_bitmap(
      s_mask_bitmap_layer,
      s_mask_gbitmap
  );

  bitmap_layer_set_compositing_mode(
      s_mask_bitmap_layer,
      GCompOpSet
  );

  layer_add_child(
      root_layer,
      bitmap_layer_get_layer(s_mask_bitmap_layer)
  );

  tick_timer_service_subscribe(
      MINUTE_UNIT,
      tick_handler
  );

  time_t now = time(NULL);
  struct tm *current_time = localtime(&now);

  tick_handler(
      current_time,
      MINUTE_UNIT
  );
}


static void window_unload(Window *window) {
  (void)window;

  tick_timer_service_unsubscribe();

  layer_destroy(s_time_layer);
  s_time_layer = NULL;

  bitmap_layer_destroy(s_mask_bitmap_layer);
  s_mask_bitmap_layer = NULL;

  bitmap_layer_destroy(s_past_bitmap_layer);
  bitmap_layer_destroy(s_present_bitmap_layer);
  bitmap_layer_destroy(s_future_bitmap_layer);

  gbitmap_destroy(s_mask_gbitmap);
  s_mask_gbitmap = NULL;

  gbitmap_destroy(s_past_gbitmap);
  gbitmap_destroy(s_present_gbitmap);
  gbitmap_destroy(s_future_gbitmap);
}


void main_window_push(void) {
  if (!s_window) {
    s_window = window_create();

    window_set_background_color(
        s_window,
        GColorWhite
    );

    window_set_window_handlers(
        s_window,
        (WindowHandlers) {
            .load = window_load,
            .unload = window_unload
        }
    );
  }

  window_stack_push(
      s_window,
      true
  );
}


void main_window_destroy(void) {
  if (!s_window) {
    return;
  }

  window_destroy(s_window);
  s_window = NULL;
}
