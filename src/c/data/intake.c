#include "intake.h"
#include <string.h>

int current_minutes(void) {
  time_t now = time(NULL);
  struct tm *tm_now = localtime(&now);
  return tm_now->tm_hour * 60 + tm_now->tm_min;
}

void send_log_event(int delta_ml, int total_ml, int minute) {
  DictionaryIterator *iter = NULL;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK || !iter) {
    return;
  }
  dict_write_int32(iter, KEY_LOG_DELTA_ML, delta_ml);
  dict_write_int32(iter, KEY_LOG_TOTAL_ML, total_ml);
  dict_write_int16(iter, KEY_LOG_MINUTE, (int16_t)minute);
  app_message_outbox_send();
}

void add_intake(PersistedState *state, int delta_ml, uint8_t *milestones_hit,
                time_t *last_intake_time, int *last_intake_amount) {
  DayData *today = ensure_today_day(state);
  
  // Store for undo functionality
  *last_intake_time = time(NULL);
  *last_intake_amount = delta_ml;
  
  int next_total = today->total_ml + delta_ml;
  if (next_total < 0) {
    next_total = 0;
  }
  today->total_ml = next_total;

  int minute = current_minutes();
  if (today->point_count == 0) {
    today->minutes[0] = (uint16_t)minute;
    // Cap cumulative_ml to uint16_t max to prevent overflow
    uint16_t capped_ml = (today->total_ml > 65535) ? 65535 : (uint16_t)today->total_ml;
    today->cumulative_ml[0] = capped_ml;
    today->point_count = 1;
  } else if (today->point_count < MAX_POINTS) {
    today->minutes[today->point_count] = (uint16_t)minute;
    // Cap cumulative_ml to uint16_t max to prevent overflow
    uint16_t capped_ml = (today->total_ml > 65535) ? 65535 : (uint16_t)today->total_ml;
    today->cumulative_ml[today->point_count] = capped_ml;
    today->point_count++;
  } else {
    today->minutes[MAX_POINTS - 1] = (uint16_t)minute;
    // Cap cumulative_ml to uint16_t max to prevent overflow
    uint16_t capped_ml = (today->total_ml > 65535) ? 65535 : (uint16_t)today->total_ml;
    today->cumulative_ml[MAX_POINTS - 1] = capped_ml;
  }

  state_save(state);
  send_log_event(delta_ml, today->total_ml, minute);
  
  // Vibration feedback
  vibes_short_pulse();
  
  // Check for milestone notifications
  int goal = state->goal_ml > 0 ? state->goal_ml : 2800;
  int progress_percent = (today->total_ml * 100) / goal;
  
  if (progress_percent >= 50 && !(*milestones_hit & (1 << 0))) {
    *milestones_hit |= (1 << 0);
    vibes_long_pulse();
  }
  if (progress_percent >= 75 && !(*milestones_hit & (1 << 1))) {
    *milestones_hit |= (1 << 1);
    vibes_long_pulse();
  }
  if (progress_percent >= 100 && !(*milestones_hit & (1 << 2))) {
    *milestones_hit |= (1 << 2);
    vibes_long_pulse();
  }
}
