#pragma once

#include <pebble.h>

#include "../animation/animation_controller.h"
#include "../settings/app_settings.h"

typedef struct {
  int displayed_value;
  int target_value;
  int modulo;

  bool initialized;
  bool animating;

  Layer *target_layer;
  const AppSettings *settings;

  AnimationController animation;
} TimeRow;

void time_row_init(
    TimeRow *row,
    Layer *target_layer,
    int modulo,
    const AppSettings *settings
);

void time_row_deinit(TimeRow *row);

void time_row_set_value(
    TimeRow *row,
    int value,
    bool animate
);

void time_row_draw(
    const TimeRow *row,
    GContext *ctx,
    int origin_y
);

/*
 * Zeichnet ausschließlich die aktuelle PRESENT-Zahl.
 * Der sichtbare Bereich wird auf den maskenfreien
 * Bounce-Korridor begrenzt.
 */
void time_row_draw_present_overlay(
    const TimeRow *row,
    GContext *ctx,
    int origin_y
);
