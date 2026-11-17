// User settings: display style and colour theme. Persisted on the watch and
// updated from the phone settings page (src/pkjs/config.js) by AppMessage.
#pragma once
#include <pebble.h>
#include "layout.h"

typedef struct {
  LayoutId layout;  // LAYOUT_A "Standard", LAYOUT_B "Large"
  uint8_t theme;    // index into the theme table (theme.h)
} Settings;

// Load persisted settings, or the defaults if none are saved yet.
Settings settings_load(void);
void settings_save(const Settings *s);

// Apply an incoming settings-page message to *s. Returns true if anything changed.
bool settings_update_from_message(Settings *s, DictionaryIterator *iter);
