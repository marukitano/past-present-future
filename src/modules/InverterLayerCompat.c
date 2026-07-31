#include "InverterLayerCompat.h"

static GColor s_foreground_color;
static GColor s_background_color;


static void inverter_update_proc(
    Layer *layer,
    GContext *ctx
) {
  GBitmap *framebuffer =
      graphics_capture_frame_buffer(ctx);

  if (!framebuffer) {
    return;
  }

  uint8_t *data =
      gbitmap_get_data(framebuffer);

  if (!data) {
    graphics_release_frame_buffer(
        ctx,
        framebuffer
    );

    return;
  }

  const GRect framebuffer_bounds =
      gbitmap_get_bounds(framebuffer);

  const GRect layer_frame =
      layer_get_frame(layer);

  const uint16_t bytes_per_row =
      gbitmap_get_bytes_per_row(framebuffer);

  int16_t left = layer_frame.origin.x;
  int16_t top = layer_frame.origin.y;

  int16_t right =
      layer_frame.origin.x
      + layer_frame.size.w;

  int16_t bottom =
      layer_frame.origin.y
      + layer_frame.size.h;

  if (left < 0) {
    left = 0;
  }

  if (top < 0) {
    top = 0;
  }

  if (right > framebuffer_bounds.size.w) {
    right = framebuffer_bounds.size.w;
  }

  if (bottom > framebuffer_bounds.size.h) {
    bottom = framebuffer_bounds.size.h;
  }

  for (
      int16_t y = top;
      y < bottom;
      ++y
  ) {
    for (
        int16_t x = left;
        x < right;
        ++x
    ) {
      const uint32_t offset =
          y * bytes_per_row + x;

      const GColor pixel = {
        .argb = data[offset]
      };

      if (
          gcolor_equal(
              pixel,
              s_foreground_color
          )
      ) {
        data[offset] =
            s_background_color.argb;
      } else if (
          gcolor_equal(
              pixel,
              s_background_color
          )
      ) {
        data[offset] =
            s_foreground_color.argb;
      }
    }
  }

  graphics_release_frame_buffer(
      ctx,
      framebuffer
  );
}


InverterLayerCompat *
inverter_layer_compat_create(
    GRect bounds
) {
  InverterLayerCompat *inverter_layer =
      malloc(sizeof(InverterLayerCompat));

  if (!inverter_layer) {
    return NULL;
  }

  inverter_layer->layer =
      layer_create(bounds);

  if (!inverter_layer->layer) {
    free(inverter_layer);
    return NULL;
  }

  layer_set_update_proc(
      inverter_layer->layer,
      inverter_update_proc
  );

  return inverter_layer;
}


void inverter_layer_compat_set_colors(
    GColor foreground,
    GColor background
) {
  s_foreground_color = foreground;
  s_background_color = background;
}


void inverter_layer_compat_destroy(
    InverterLayerCompat *inverter_layer
) {
  if (!inverter_layer) {
    return;
  }

  if (inverter_layer->layer) {
    layer_destroy(
        inverter_layer->layer
    );
  }

  free(inverter_layer);
}


Layer *inverter_layer_compat_get_layer(
    InverterLayerCompat *inverter_layer
) {
  if (!inverter_layer) {
    return NULL;
  }

  return inverter_layer->layer;
}
