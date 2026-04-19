#pragma once
#include "../state/state.h"

// Statistics calculations
int calculate_weekly_avg(PersistedState *state);
int find_best_day(const PersistedState *state);
int count_logged_days(const PersistedState *state);
