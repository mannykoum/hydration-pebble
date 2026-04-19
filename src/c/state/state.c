#include "state.h"
#include "c/data/stats.h"
#include <string.h>

void state_save(PersistedState *state) {
  persist_write_data(STORAGE_KEY_STATE, state, sizeof(PersistedState));
}

void state_load(PersistedState *state) {
  if (persist_exists(STORAGE_KEY_STATE)) {
    persist_read_data(STORAGE_KEY_STATE, state, sizeof(PersistedState));
  } else {
    memset(state, 0, sizeof(PersistedState));
    state->goal_ml = 2800;
    state->amounts_ml[0] = 250;
    state->amounts_ml[1] = 500;
    state->amounts_ml[2] = 750;
    state->amounts_ml[3] = 1000;
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

int find_best_day(const PersistedState *state) {
  int best = 0;
  for (int i = 0; i < MAX_DAYS; i++) {
    if (state->days[i].total_ml > best) {
      best = state->days[i].total_ml;
    }
  }
  return best;
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
