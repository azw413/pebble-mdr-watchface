#include "settings.h"
#include "theme.h"

#define PERSIST_KEY_SETTINGS 1
#define SETTINGS_VERSION 2

// Version 1 had only the first three fields; it is migrated on load.
typedef struct __attribute__((packed)) {
  uint8_t version;
  uint8_t layout;
  uint8_t theme;
  uint8_t anim_every;   // added in version 2
} StoredSettings;

// Choices offered by the settings page (src/pkjs/config.js).
static bool valid_interval(int32_t m) {
  return m == 0 || m == 1 || m == 5 || m == 10 || m == 15 || m == 30 || m == 60;
}

// Defaults until the user saves settings. MDR_LAYOUT=B / MDR_THEME=<n> in the
// build environment change them, for emulator previews (see wscript).
#ifdef MDR_LAYOUT_B
#define DEFAULT_LAYOUT LAYOUT_B
#else
#define DEFAULT_LAYOUT LAYOUT_A
#endif
#ifndef MDR_THEME
#define MDR_THEME THEME_DEFAULT
#endif

Settings settings_load(void) {
  Settings s = {.layout = DEFAULT_LAYOUT, .theme = MDR_THEME, .anim_every = ANIM_EVERY_DEFAULT};
  StoredSettings st = {0};
  if (!persist_exists(PERSIST_KEY_SETTINGS)) return s;
  const int n = persist_read_data(PERSIST_KEY_SETTINGS, &st, sizeof(st));
  if (n < 3 || st.version < 1 || st.version > SETTINGS_VERSION) return s;
  s.layout = st.layout == LAYOUT_B ? LAYOUT_B : LAYOUT_A;
  s.theme = st.theme < THEME_COUNT ? st.theme : THEME_DEFAULT;
  if (st.version >= 2 && n >= (int)sizeof(st) && valid_interval(st.anim_every)) {
    s.anim_every = st.anim_every;
  }
  return s;
}

void settings_save(const Settings *s) {
  StoredSettings st = {.version = SETTINGS_VERSION, .layout = s->layout, .theme = s->theme,
                       .anim_every = s->anim_every};
  persist_write_data(PERSIST_KEY_SETTINGS, &st, sizeof(st));
}

// Clay sends selects as strings and colours as integers; accept either.
static int32_t tuple_int(const Tuple *t) {
  switch (t->type) {
    case TUPLE_CSTRING: return atoi(t->value->cstring);
    case TUPLE_INT:
      return t->length == 1 ? t->value->int8 : t->length == 2 ? t->value->int16 : t->value->int32;
    case TUPLE_UINT:
      return t->length == 1 ? t->value->uint8 : t->length == 2 ? t->value->uint16
                                                               : (int32_t)t->value->uint32;
    default: return 0;
  }
}

bool settings_update_from_message(Settings *s, DictionaryIterator *iter) {
  const Settings before = *s;
  Tuple *t = dict_find(iter, MESSAGE_KEY_Layout);
  if (t) s->layout = tuple_int(t) == 1 ? LAYOUT_B : LAYOUT_A;
  t = dict_find(iter, MESSAGE_KEY_Colour);
  if (t) s->theme = theme_from_rgb((uint32_t)tuple_int(t));
  t = dict_find(iter, MESSAGE_KEY_Interval);
  if (t && valid_interval(tuple_int(t))) s->anim_every = (uint8_t)tuple_int(t);
  return s->layout != before.layout || s->theme != before.theme ||
         s->anim_every != before.anim_every;
}

bool settings_anim_due(const Settings *s, const struct tm *t) {
  if (s->anim_every == 0) return false;
  // Aligned to the clock: every 15 min means :00, :15, :30 and :45.
  return (t->tm_hour * 60 + t->tm_min) % s->anim_every == 0;
}
