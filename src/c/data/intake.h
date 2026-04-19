#pragma once
#include "../state/state.h"

#define KEY_LOG_DELTA_ML 6
#define KEY_LOG_TOTAL_ML 7
#define KEY_LOG_MINUTE 8

// Intake operations
void add_intake(PersistedState *state, int delta_ml, uint8_t *milestones_hit, 
                time_t *last_intake_time, int *last_intake_amount);
void send_log_event(int delta_ml, int total_ml, int minute);
int current_minutes(void);
