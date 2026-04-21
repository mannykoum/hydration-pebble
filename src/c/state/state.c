#include "state.h"
#include <string.h>

typedef struct {
  int goal_ml;
  int amounts_ml[MAX_AMOUNTS];
  uint8_t unit;
  int current_streak;
} SettingsBlock;

void state_save(PersistedState *state) {
  uint8_t version = STORAGE_VERSION;
  int ret = persist_write_data(STORAGE_KEY_VERSION, &version, sizeof(version));
  if (ret < 0) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "persist version failed: %d", ret);
  }
  
  SettingsBlock settings = {
    .goal_ml = state->goal_ml,
    .unit = state->unit,
    .current_streak = state->current_streak
  };
  memcpy(settings.amounts_ml, state->amounts_ml, sizeof(settings.amounts_ml));
  ret = persist_write_data(STORAGE_KEY_SETTINGS, &settings, sizeof(SettingsBlock));
  if (ret < 0) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "persist settings failed: %d", ret);
  }
  
  for (int i = 0; i < MAX_DAYS; i++) {
    ret = persist_write_data(STORAGE_KEY_DAY_BASE + i, &state->days[i], sizeof(DayData));
    if (ret < 0) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "persist day %d failed: %d", i, ret);
    }
  }
}

// Valid date_key range: 20150101 to 20351231 (Pebble era)
#define DATE_KEY_MIN 20150101
#define DATE_KEY_MAX 20351231
// No human drinks more than 20 liters in a day
#define MAX_DAILY_ML 20000

static void sanitize_state(PersistedState *state) {
  // Validate goal
  if (state->goal_ml <= 0 || state->goal_ml > MAX_DAILY_ML) {
    state->goal_ml = 2800;
  }

  // Validate amounts (allow negative for "remove" options)
  int defaults[] = {250, 500, 750, 1000, 237, -237};
  for (int i = 0; i < MAX_AMOUNTS; i++) {
    if (state->amounts_ml[i] == 0 ||
        state->amounts_ml[i] > MAX_DAILY_ML ||
        state->amounts_ml[i] < -MAX_DAILY_ML) {
      state->amounts_ml[i] = defaults[i];
    }
  }

  if (state->unit > UNIT_PINTS) {
    state->unit = UNIT_ML;
  }

  if (state->current_streak < 0) {
    state->current_streak = 0;
  }

  // Validate each day entry
  for (int i = 0; i < MAX_DAYS; i++) {
    DayData *d = &state->days[i];
    bool valid = d->date_key >= DATE_KEY_MIN && d->date_key <= DATE_KEY_MAX
              && d->total_ml >= 0 && d->total_ml <= MAX_DAILY_ML
              && d->point_count <= MAX_POINTS;
    if (!valid) {
      memset(d, 0, sizeof(DayData));
    }
  }
}

void state_load(PersistedState *state) {
  if (persist_exists(STORAGE_KEY_VERSION)) {
    uint8_t version = 0;
    persist_read_data(STORAGE_KEY_VERSION, &version, sizeof(version));
    
    if (version != STORAGE_VERSION) {
      APP_LOG(APP_LOG_LEVEL_WARNING, "Version mismatch: %d != %d, resetting", version, STORAGE_VERSION);
      memset(state, 0, sizeof(PersistedState));
      state->goal_ml = 2800;
      state->amounts_ml[0] = 250;
      state->amounts_ml[1] = 500;
      state->amounts_ml[2] = 750;
      state->amounts_ml[3] = 1000;
      state->amounts_ml[4] = 237;
      state->amounts_ml[5] = -237;
      state->unit = UNIT_ML;
      return;
    }
    
    SettingsBlock settings;
    persist_read_data(STORAGE_KEY_SETTINGS, &settings, sizeof(SettingsBlock));
    state->goal_ml = settings.goal_ml;
    state->unit = settings.unit;
    state->current_streak = settings.current_streak;
    memcpy(state->amounts_ml, settings.amounts_ml, sizeof(state->amounts_ml));
    
    for (int i = 0; i < MAX_DAYS; i++) {
      persist_read_data(STORAGE_KEY_DAY_BASE + i, &state->days[i], sizeof(DayData));
    }
    
    sanitize_state(state);
  } else {
    memset(state, 0, sizeof(PersistedState));
    state->goal_ml = 2800;
    state->amounts_ml[0] = 250;
    state->amounts_ml[1] = 500;
    state->amounts_ml[2] = 750;
    state->amounts_ml[3] = 1000;
    state->amounts_ml[4] = 237;
    state->amounts_ml[5] = -237;
    state->unit = UNIT_ML;
  }
}

int day_key_from_time(time_t t) {
  struct tm *tm_now = localtime(&t);
  return (tm_now->tm_year + 1900) * 10000 + (tm_now->tm_mon + 1) * 100 + tm_now->tm_mday;
}

int find_day_index(const PersistedState *state, int key) {
  for (int i = 0; i < MAX_DAYS; i++) {
    if (state->days[i].date_key == key) {
      return i;
    }
  }
  return -1;
}

int oldest_day_index(const PersistedState *state) {
  int oldest = -1;
  for (int i = 0; i < MAX_DAYS; i++) {
    if (state->days[i].date_key == 0) {
      return i;
    }
    if (oldest < 0 || state->days[i].date_key < state->days[oldest].date_key) {
      oldest = i;
    }
  }
  return oldest;
}

DayData* ensure_today_day(PersistedState *state) {
  time_t now = time(NULL);
  int key = day_key_from_time(now);
  int idx = find_day_index(state, key);
  if (idx >= 0) {
    return &state->days[idx];
  }

  // Check if yesterday met goal to update streak
  time_t yesterday_time = now - 24 * 60 * 60;
  int yesterday_key = day_key_from_time(yesterday_time);
  int yesterday_idx = find_day_index(state, yesterday_key);
  int goal = state->goal_ml > 0 ? state->goal_ml : 2800;
  
  if (yesterday_idx >= 0 && state->days[yesterday_idx].total_ml >= goal) {
    state->current_streak++;
  } else {
    state->current_streak = 0;
  }

  idx = oldest_day_index(state);
  memset(&state->days[idx], 0, sizeof(DayData));
  state->days[idx].date_key = key;
  return &state->days[idx];
}

void reset_if_new_day(PersistedState *state, uint8_t *milestones_hit) {
  (void)ensure_today_day(state);
  *milestones_hit = 0;
}

DayData* day_by_offset(PersistedState *state, int offset) {
  time_t now = time(NULL) - (time_t)offset * 24 * 60 * 60;
  int key = day_key_from_time(now);
  int idx = find_day_index(state, key);
  if (idx < 0) {
    return NULL;
  }
  return &state->days[idx];
}

int day_total(PersistedState *state, int offset) {
  DayData *day = day_by_offset(state, offset);
  return day ? day->total_ml : 0;
}
