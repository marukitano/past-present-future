#pragma once

#include <pebble.h>

typedef struct {
  Layer *layer;
} InverterLayerCompat;

InverterLayerCompat *inverter_layer_compat_create(
    GRect bounds
);

void inverter_layer_compat_set_colors(
    GColor foreground,
    GColor background
);

void inverter_layer_compat_destroy(
    InverterLayerCompat *inverter_layer
);

Layer *inverter_layer_compat_get_layer(
    InverterLayerCompat *inverter_layer
);
