#include <pebble.h>

#include "windows/main_window.h"

static void init(void) {
  main_window_push();
}

static void deinit(void) {
  main_window_destroy();
}

int main(void) {
  init();
  app_event_loop();
  deinit();

  return 0;
}
