#pragma once
#include <pebble.h>
#include "../state/state.h"

typedef enum {
  VIEW_MAIN = 0,
  VIEW_AMOUNT = 1,
  VIEW_DETAIL = 2,
  VIEW_WEEKLY = 3,
  VIEW_STATS = 4,
  VIEW_COUNT
} MainView;

// UI Color scheme
#ifdef PBL_COLOR
#define UI_BG GColorWhite
#define UI_TEXT GColorBlack
#define UI_MUTED GColorDarkGray
#define UI_ACCENT GColorCyan
#define UI_ACCENT_ALT GColorBlue
#define UI_POSITIVE GColorGreen
#else
#define UI_BG GColorWhite
#define UI_TEXT GColorBlack
#define UI_MUTED GColorBlack
#define UI_ACCENT GColorBlack
#define UI_ACCENT_ALT GColorBlack
#define UI_POSITIVE GColorBlack
#endif

// Typography scale
#define FONT_DISPLAY fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD)
#define FONT_TITLE fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD)
#define FONT_BODY fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD)
#define FONT_CAPTION fonts_get_system_font(FONT_KEY_GOTHIC_14)
#define FONT_SMALL fonts_get_system_font(FONT_KEY_GOTHIC_09)

// UI state structure (passed to drawing functions)
typedef struct {
  PersistedState *state;
  MainView view;
  bool edit_goal;
  bool edit_amount;
  bool selecting_day;
  int selected_amount;
  int selected_day_offset;
  bool celebrating;
  int celebration_counter;
  bool show_undo_message;
  uint8_t milestones_hit;
  bool anim_on;
  bool celebration_played_today;
} UIState;

// Drawing functions
void draw_main_view(GContext *ctx, GRect bounds, UIState *ui_state);
void draw_amount_view(GContext *ctx, GRect bounds, UIState *ui_state);
void draw_detail_view(GContext *ctx, GRect bounds, UIState *ui_state);
void draw_weekly_view(GContext *ctx, GRect bounds, UIState *ui_state);
void draw_stats_view(GContext *ctx, GRect bounds, UIState *ui_state);
void draw_celebration(GContext *ctx, GRect bounds, UIState *ui_state);

// Utility drawing functions
void draw_progress_bar(GContext *ctx, GRect frame, int numerator, int denominator, const char *label, bool anim_on);
