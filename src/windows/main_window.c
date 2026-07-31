#include <pebble.h>

#include "../info/info_display.h"
#include "../modules/InverterLayerCompat.h"
#include "../render/time_row.h"
#include "../settings/app_settings.h"
#include "main_window.h"

#if !defined(PBL_PLATFORM_EMERY)
#error "Past Present Future supports only Pebble Time 2 / Emery."
#endif

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

static BitmapLayer *s_past_bitmap_layer;
static BitmapLayer *s_present_bitmap_layer;
static BitmapLayer *s_swiss_protected_bitmap_layer;
static BitmapLayer *s_future_bitmap_layer;

static GBitmap *s_past_gbitmap;
static GBitmap *s_present_gbitmap;
static GBitmap *s_present_swiss_gbitmap;
static GBitmap *s_swiss_protected_gbitmap;
static GBitmap *s_future_gbitmap;
static GBitmap *s_mask_gbitmap;

static InverterLayerCompat *s_inverter_layer;

static bool s_dark_mode;
static bool s_wrist_shake_locked;
static bool s_shake_service_subscribed;

static AppTimer *s_wrist_shake_timer;

static void apply_theme_mode(void);


static void apply_present_variant(void) {
  if (!s_present_bitmap_layer) {
    return;
  }

  const AppSettings *settings =
      app_settings_get();

  GBitmap *bitmap = s_present_gbitmap;

  if (
      settings
      && settings->show_swiss_emblem
      && s_present_swiss_gbitmap
  ) {
    bitmap = s_present_swiss_gbitmap;
  }

  bitmap_layer_set_bitmap(
      s_present_bitmap_layer,
      bitmap
  );

  layer_mark_dirty(
      bitmap_layer_get_layer(
          s_present_bitmap_layer
      )
  );

  if (s_swiss_protected_bitmap_layer) {
    Layer *protected_layer =
        bitmap_layer_get_layer(
            s_swiss_protected_bitmap_layer
        );

    layer_set_hidden(
        protected_layer,
        !(
          settings
          && settings->show_swiss_emblem
        )
    );

    layer_mark_dirty(protected_layer);
  }
}


static void settings_changed_handler(void) {
  apply_present_variant();
  apply_theme_mode();

  time_t now = time(NULL);

  struct tm *current_time =
      localtime(&now);

  info_display_update(current_time);
}


static void mark_inverter_dirty(void) {
  if (
      !s_dark_mode
      || !s_inverter_layer
  ) {
    return;
  }

  Layer *layer =
      inverter_layer_compat_get_layer(
          s_inverter_layer
      );

  if (layer) {
    layer_mark_dirty(layer);
  }
}


static void apply_dark_mode(void) {
  if (!s_inverter_layer) {
    return;
  }

  Layer *layer =
      inverter_layer_compat_get_layer(
          s_inverter_layer
      );

  if (!layer) {
    return;
  }

  layer_set_hidden(
      layer,
      !s_dark_mode
  );

  layer_mark_dirty(layer);
}


static void wrist_shake_unlock(
    void *context
) {
  (void)context;

  s_wrist_shake_timer = NULL;
  s_wrist_shake_locked = false;
}


static void wrist_shake_handler(
    AccelAxisType axis,
    int32_t direction
) {
  (void)axis;
  (void)direction;

  /*
   * Ein kräftiges Schütteln kann mehrere Tap-Events
   * erzeugen. Deshalb kurze Sperre gegen Doppelschalten.
   */
  const AppSettings *settings =
      app_settings_get();

  if (
      !settings
      || settings->theme_mode
          != PPF_THEME_SHAKE
      || s_wrist_shake_locked
  ) {
    return;
  }

  s_wrist_shake_locked = true;
  s_dark_mode = !s_dark_mode;

  apply_dark_mode();

  s_wrist_shake_timer =
      app_timer_register(
          700,
          wrist_shake_unlock,
          NULL
      );
}


static void cancel_wrist_shake_timer(void) {
  if (s_wrist_shake_timer) {
    app_timer_cancel(
        s_wrist_shake_timer
    );

    s_wrist_shake_timer = NULL;
  }

  s_wrist_shake_locked = false;
}


static void set_shake_subscription(
    bool enabled
) {
  if (
      enabled
      && !s_shake_service_subscribed
  ) {
    accel_tap_service_subscribe(
        wrist_shake_handler
    );

    s_shake_service_subscribed = true;
  } else if (
      !enabled
      && s_shake_service_subscribed
  ) {
    accel_tap_service_unsubscribe();
    s_shake_service_subscribed = false;
  }

  if (!enabled) {
    cancel_wrist_shake_timer();
  }
}


static void apply_theme_mode(void) {
  const AppSettings *settings =
      app_settings_get();

  if (
      !settings
      || !s_inverter_layer
  ) {
    return;
  }

  const PpfThemeMode theme_mode =
      (PpfThemeMode)settings->theme_mode;

  set_shake_subscription(
      theme_mode == PPF_THEME_SHAKE
  );

  switch (theme_mode) {
    case PPF_THEME_LIGHT:
      s_dark_mode = false;
      break;

    case PPF_THEME_DARK:
      s_dark_mode = true;
      break;

    case PPF_THEME_SHAKE:
    default:
      /*
       * Beim Start ist der Shake-Modus hell.
       * Danach bleibt der durch Schütteln gewählte
       * Zustand erhalten.
       */
      break;
  }

  apply_dark_mode();
}


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

  mark_inverter_dirty();
}


static void tick_handler(
    struct tm *tick_time,
    TimeUnits units_changed
) {
  (void)units_changed;

  info_display_update(tick_time);

  time_row_set_value(
      &s_hour_row,
      tick_time->tm_hour,
      true
  );

  time_row_set_value(
      &s_minute_row,
      tick_time->tm_min,
      true
  );
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

  s_present_swiss_gbitmap =
      gbitmap_create_with_resource(
          RESOURCE_ID_PRESENT_SWISS
      );

  s_swiss_protected_gbitmap =
      gbitmap_create_with_resource(
          RESOURCE_ID_PRESENT_SWISS_PROTECTED
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

  const AppSettings *settings =
      app_settings_get();

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
      settings
  );

  time_row_init(
      &s_minute_row,
      s_time_layer,
      60,
      settings
  );

  s_mask_gbitmap =
      gbitmap_create_with_resource(
          RESOURCE_ID_MASK
      );

  /*
   * Als letztes erzeugt, damit Datum und Temperatur
   * oberhalb der Zeitmaske liegen.
   */
  info_display_init(root_layer);

  inverter_layer_compat_set_colors(
      GColorBlack,
      GColorWhite
  );

  s_inverter_layer =
      inverter_layer_compat_create(
          GRect(
              0,
              0,
              PBL_DISPLAY_WIDTH,
              PBL_DISPLAY_HEIGHT
          )
      );

  if (s_inverter_layer) {
    Layer *inverter_layer =
        inverter_layer_compat_get_layer(
            s_inverter_layer
        );

    layer_add_child(
        root_layer,
        inverter_layer
    );

    layer_set_hidden(
        inverter_layer,
        true
    );
  }

  /*
   * Keeps the red emblem and white cross unchanged
   * above the black/white inverter.
   */
  s_swiss_protected_bitmap_layer =
      create_bitmap_layer(
          root_layer,
          s_swiss_protected_gbitmap,
          GRect(
              PRESENT_COLUMN_X,
              COLUMN_Y,
              COLUMN_WIDTH,
              COLUMN_HEIGHT
          )
      );

  bitmap_layer_set_compositing_mode(
      s_swiss_protected_bitmap_layer,
      GCompOpSet
  );

  s_dark_mode = false;
  s_wrist_shake_locked = false;
  s_shake_service_subscribed = false;

  app_settings_set_changed_handler(
      settings_changed_handler
  );

  settings_changed_handler();

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

  set_shake_subscription(false);

  if (s_inverter_layer) {
    inverter_layer_compat_destroy(
        s_inverter_layer
    );

    s_inverter_layer = NULL;
  }

  s_dark_mode = false;

  app_settings_set_changed_handler(NULL);

  info_display_deinit();

  time_row_deinit(&s_hour_row);
  time_row_deinit(&s_minute_row);

  layer_destroy(s_time_layer);
  s_time_layer = NULL;

  bitmap_layer_destroy(s_past_bitmap_layer);
  bitmap_layer_destroy(s_present_bitmap_layer);
  bitmap_layer_destroy(
      s_swiss_protected_bitmap_layer
  );
  bitmap_layer_destroy(s_future_bitmap_layer);

  gbitmap_destroy(s_mask_gbitmap);
  s_mask_gbitmap = NULL;

  gbitmap_destroy(s_past_gbitmap);
  gbitmap_destroy(s_present_gbitmap);
  gbitmap_destroy(s_present_swiss_gbitmap);
  gbitmap_destroy(
      s_swiss_protected_gbitmap
  );
  gbitmap_destroy(s_future_gbitmap);

  s_past_gbitmap = NULL;
  s_present_gbitmap = NULL;
  s_present_swiss_gbitmap = NULL;
  s_swiss_protected_gbitmap = NULL;
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
