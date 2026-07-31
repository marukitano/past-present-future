#include <pebble.h>

#include "settings/app_settings.h"
#include "windows/main_window.h"

static void init(void) {
  app_settings_init();
  main_window_push();
}

static void deinit(void) {
  main_window_destroy();
  app_settings_deinit();
}

int main(void) {
  init();
  app_event_loop();
  deinit();

  return 0;
}
