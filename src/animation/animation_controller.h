#pragma once

#include <pebble.h>

typedef void (*AnimationControllerStoppedHandler)(
    bool finished,
    void *context
);

typedef struct {
  Animation *animation;
  AnimationProgress progress;

  Layer *target_layer;

  AnimationControllerStoppedHandler stopped_handler;
  void *handler_context;

  bool running;
  bool suppress_callback;
} AnimationController;

void animation_controller_init(
    AnimationController *controller,
    Layer *target_layer,
    AnimationControllerStoppedHandler stopped_handler,
    void *handler_context
);

void animation_controller_deinit(
    AnimationController *controller
);

bool animation_controller_start(
    AnimationController *controller,
    uint32_t duration_ms
);

void animation_controller_cancel(
    AnimationController *controller
);

bool animation_controller_is_running(
    const AnimationController *controller
);

AnimationProgress animation_controller_get_progress(
    const AnimationController *controller
);
