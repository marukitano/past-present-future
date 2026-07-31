#include <pebble.h>

#include "../render/time_row.h"
#include "../settings/app_settings.h"
#include "main_window.h"

#if !defined(PBL_PLATFORM_EMERY)
#error "Past Present Future supports only Pebble Time 2 / Emery."
#endif

/*
 * Für schnelles Testen auf 1 setzen:
 * Die untere Zeile zeigt Sekunden und animiert jede Sekunde.
 *
 * Vor einem Commit wieder auf 0 setzen.
 */
#define PPF_EFFECT_DEMO_MODE 1

#define HOUR_ROW_Y 18
#define MINUTE_ROW_Y 45

#define COLUMN_WIDTH 46
#define COLUMN_HEIGHT PBL_DISPLAY_HEIGHT
#define COLUMN_Y 0

#define PAST_COLUMN_X (-4)
#define PRESENT_COLUMN_X 76
#define FUTURE_COLUMN_X 156

static Window *s_window;
static Layer *s_time_layer;

static TimeRow s_hour_row;
static TimeRow s_minute_row;

static const AppSettings *s_settings;

static BitmapLayer *s_past_bitmap_layer;
static BitmapLayer *s_present_bitmap_layer;
static BitmapLayer *s_future_bitmap_layer;

static GBitmap *s_past_gbitmap;
static GBitmap *s_present_gbitmap;
static GBitmap *s_future_gbitmap;
static GBitmap *s_mask_gbitmap;


static void time_layer_update_proc(
    Layer *layer,
    GContext *ctx
) {
  (void)layer;

  graphics_context_set_fill_color(
      ctx,
      GColorBlack
  );

  time_row_draw(
      &s_hour_row,
      ctx,
      HOUR_ROW_Y
  );

  time_row_draw(
      &s_minute_row,
      ctx,
      MINUTE_ROW_Y
  );

  /*
   * Die Maske wird bei jedem Animationsframe erneut über
   * Zahlen und Hintergrundgrafiken gezeichnet.
   */
  if (s_mask_gbitmap) {
    graphics_context_set_compositing_mode(
        ctx,
        GCompOpSet
    );

    graphics_draw_bitmap_in_rect(
        ctx,
        s_mask_gbitmap,
        GRect(
            0,
            0,
            PBL_DISPLAY_WIDTH,
            PBL_DISPLAY_HEIGHT
        )
    );
  }

  /*
   * PRESENT wird im dynamischen Bounce-Korridor
   * nochmals unmaskiert über die Maske gezeichnet.
   */
  graphics_context_set_fill_color(
      ctx,
      GColorBlack
  );

  time_row_draw_present_overlay(
      &s_hour_row,
      ctx,
      HOUR_ROW_Y
  );

  time_row_draw_present_overlay(
      &s_minute_row,
      ctx,
      MINUTE_ROW_Y
  );
}


static void tick_handler(
    struct tm *tick_time,
    TimeUnits units_changed
) {
  (void)units_changed;

#if PPF_EFFECT_DEMO_MODE
  time_row_set_value(
      &s_hour_row,
      tick_time->tm_hour,
      false
  );

  time_row_set_value(
      &s_minute_row,
      tick_time->tm_sec,
      true
  );
#else
  time_row_set_value(
      &s_hour_row,
      tick_time->tm_hour,
      s_settings->animate_hours
  );

  time_row_set_value(
      &s_minute_row,
      tick_time->tm_min,
      s_settings->animate_minutes
  );
#endif
}


static BitmapLayer *create_bitmap_layer(
    Layer *root_layer,
    GBitmap *bitmap,
    GRect frame
) {
  BitmapLayer *bitmap_layer =
      bitmap_layer_create(frame);

  bitmap_layer_set_bitmap(
      bitmap_layer,
      bitmap
  );

  layer_add_child(
      root_layer,
      bitmap_layer_get_layer(bitmap_layer)
  );

  return bitmap_layer;
}


static void create_column_layers(
    Layer *root_layer
) {
  s_past_gbitmap =
      gbitmap_create_with_resource(
          RESOURCE_ID_PAST
      );

  s_present_gbitmap =
      gbitmap_create_with_resource(
          RESOURCE_ID_PRESENT
      );

  s_future_gbitmap =
      gbitmap_create_with_resource(
          RESOURCE_ID_FUTURE
      );

  s_past_bitmap_layer = create_bitmap_layer(
      root_layer,
      s_past_gbitmap,
      GRect(
          PAST_COLUMN_X,
          COLUMN_Y,
          COLUMN_WIDTH,
          COLUMN_HEIGHT
      )
  );

  s_present_bitmap_layer = create_bitmap_layer(
      root_layer,
      s_present_gbitmap,
      GRect(
          PRESENT_COLUMN_X,
          COLUMN_Y,
          COLUMN_WIDTH,
          COLUMN_HEIGHT
      )
  );

  s_future_bitmap_layer = create_bitmap_layer(
      root_layer,
      s_future_gbitmap,
      GRect(
          FUTURE_COLUMN_X,
          COLUMN_Y,
          COLUMN_WIDTH,
          COLUMN_HEIGHT
      )
  );
}


static void window_load(Window *window) {
  Layer *root_layer =
      window_get_root_layer(window);

  s_settings = app_settings_get();

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

  time_row_init(
      &s_hour_row,
      s_time_layer,
      24,
      s_settings
  );

  time_row_init(
      &s_minute_row,
      s_time_layer,
      60,
      s_settings
  );

  s_mask_gbitmap =
      gbitmap_create_with_resource(
          RESOURCE_ID_MASK
      );

#if PPF_EFFECT_DEMO_MODE
  tick_timer_service_subscribe(
      SECOND_UNIT,
      tick_handler
  );
#else
  tick_timer_service_subscribe(
      MINUTE_UNIT,
      tick_handler
  );
#endif

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

  time_row_deinit(&s_hour_row);
  time_row_deinit(&s_minute_row);

  layer_destroy(s_time_layer);
  s_time_layer = NULL;

  bitmap_layer_destroy(s_past_bitmap_layer);
  bitmap_layer_destroy(s_present_bitmap_layer);
  bitmap_layer_destroy(s_future_bitmap_layer);

  gbitmap_destroy(s_mask_gbitmap);
  s_mask_gbitmap = NULL;

  gbitmap_destroy(s_past_gbitmap);
  gbitmap_destroy(s_present_gbitmap);
  gbitmap_destroy(s_future_gbitmap);

  s_past_gbitmap = NULL;
  s_present_gbitmap = NULL;
  s_future_gbitmap = NULL;
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
