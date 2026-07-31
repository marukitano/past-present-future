#include "animation_controller.h"


static void animation_update(
    Animation *animation,
    const AnimationProgress progress
) {
  AnimationController *controller =
      animation_get_context(animation);

  if (!controller) {
    return;
  }

  controller->progress = progress;

  if (controller->target_layer) {
    layer_mark_dirty(controller->target_layer);
  }
}


static const AnimationImplementation
    ANIMATION_IMPLEMENTATION = {
  .update = animation_update
};


static void animation_stopped(
    Animation *animation,
    bool finished,
    void *context
) {
  AnimationController *controller = context;

  if (!controller) {
    animation_destroy(animation);
    return;
  }

  controller->running = false;

  if (finished) {
    controller->progress =
        ANIMATION_NORMALIZED_MAX;
  }

  controller->animation = NULL;

  animation_destroy(animation);

  if (
      !controller->suppress_callback
      && controller->stopped_handler
  ) {
    controller->stopped_handler(
        finished,
        controller->handler_context
    );
  }

  if (controller->target_layer) {
    layer_mark_dirty(controller->target_layer);
  }
}


void animation_controller_init(
    AnimationController *controller,
    Layer *target_layer,
    AnimationControllerStoppedHandler stopped_handler,
    void *handler_context
) {
  if (!controller) {
    return;
  }

  *controller = (AnimationController) {
    .animation = NULL,
    .progress = ANIMATION_NORMALIZED_MIN,

    .target_layer = target_layer,

    .stopped_handler = stopped_handler,
    .handler_context = handler_context,

    .running = false,
    .suppress_callback = false
  };
}


void animation_controller_deinit(
    AnimationController *controller
) {
  if (!controller) {
    return;
  }

  controller->suppress_callback = true;
  animation_controller_cancel(controller);

  controller->target_layer = NULL;
  controller->stopped_handler = NULL;
  controller->handler_context = NULL;
}


bool animation_controller_start(
    AnimationController *controller,
    uint32_t duration_ms
) {
  if (!controller) {
    return false;
  }

  animation_controller_cancel(controller);

  Animation *animation = animation_create();

  if (!animation) {
    return false;
  }

  controller->animation = animation;
  controller->progress = ANIMATION_NORMALIZED_MIN;
  controller->running = true;
  controller->suppress_callback = false;

  const bool configured =
      animation_set_implementation(
          animation,
          &ANIMATION_IMPLEMENTATION
      )
      && animation_set_duration(
          animation,
          duration_ms
      )
      && animation_set_curve(
          animation,
          AnimationCurveLinear
      )
      && animation_set_handlers(
          animation,
          (AnimationHandlers) {
            .stopped = animation_stopped
          },
          controller
      );

  if (!configured) {
    controller->animation = NULL;
    controller->running = false;

    animation_destroy(animation);
    return false;
  }

  if (!animation_schedule(animation)) {
    controller->animation = NULL;
    controller->running = false;

    animation_destroy(animation);
    return false;
  }

  return true;
}


void animation_controller_cancel(
    AnimationController *controller
) {
  if (!controller || !controller->animation) {
    return;
  }

  if (
      animation_is_scheduled(controller->animation)
  ) {
    animation_unschedule(controller->animation);
    return;
  }

  animation_destroy(controller->animation);

  controller->animation = NULL;
  controller->running = false;
}


bool animation_controller_is_running(
    const AnimationController *controller
) {
  return controller && controller->running;
}


AnimationProgress animation_controller_get_progress(
    const AnimationController *controller
) {
  if (!controller) {
    return ANIMATION_NORMALIZED_MIN;
  }

  return controller->progress;
}
