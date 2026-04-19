#include "stats.h"

int calculate_weekly_avg(PersistedState *state) {
  int sum = 0;
  int count = 0;
  for (int i = 0; i < 7; i++) {
    int total = day_total(state, i);
    if (total > 0) {
      sum += total;
      count++;
    }
  }
  return count > 0 ? sum / count : 0;
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

int count_logged_days(const PersistedState *state) {
  int count = 0;
  for (int i = 0; i < MAX_DAYS; i++) {
    if (state->days[i].total_ml > 0) {
      count++;
    }
  }
  return count;
}
