#pragma once
#include <pebble.h>

#define STORAGE_VERSION 1
#define STORAGE_KEY_VERSION 0
#define STORAGE_KEY_SETTINGS 1
#define STORAGE_KEY_DAY_BASE 2

#define MAX_AMOUNTS 6
#define MAX_DAYS 14
#define MAX_POINTS 16

typedef enum {
  UNIT_ML = 0,
  UNIT_CUPS = 1,
  UNIT_PINTS = 2
} Unit;

typedef struct {
  int date_key;
  int total_ml;
  uint8_t point_count;
  uint16_t minutes[MAX_POINTS];
  uint16_t cumulative_ml[MAX_POINTS];
} DayData;

typedef struct {
  int goal_ml;
  int amounts_ml[MAX_AMOUNTS];
  uint8_t unit;
  int current_streak;
  DayData days[MAX_DAYS];
} PersistedState;

// State management functions
void state_save(PersistedState *state);
void state_load(PersistedState *state);

// Day operations
int day_key_from_time(time_t t);
int find_day_index(const PersistedState *state, int key);
int oldest_day_index(const PersistedState *state);
DayData* ensure_today_day(PersistedState *state);
void reset_if_new_day(PersistedState *state, uint8_t *milestones_hit);
DayData* day_by_offset(PersistedState *state, int offset);
int day_total(PersistedState *state, int offset);
