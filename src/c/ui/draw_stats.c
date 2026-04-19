#include "ui.h"
#include "../data/stats.h"
#include "../state/state.h"
#include <stdio.h>

void draw_stats_view(GContext *ctx, GRect bounds, UIState *ui_state) {
  int weekly_avg = calculate_weekly_avg(ui_state->state);
  int best_day = find_best_day(ui_state->state);
  int logged_days = count_logged_days(ui_state->state);
  int streak = ui_state->state->current_streak;

  // If today's goal is met, display streak includes today
  DayData *today = day_by_offset(ui_state->state, 0);
  if (today && today->total_ml >= ui_state->state->goal_ml) {
    streak = ui_state->state->current_streak + 1;
  }

  graphics_context_set_text_color(ctx, UI_TEXT);

  int y = 8;
  int gap = 4;

  // Weekly Average
  graphics_draw_text(ctx, "Weekly Avg", FONT_CAPTION,
                     GRect(8, y, bounds.size.w - 16, 16),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  y += 16;
  char avg_text[16];
  snprintf(avg_text, sizeof(avg_text), "%d ml", weekly_avg);
  graphics_draw_text(ctx, avg_text, FONT_BODY,
                     GRect(8, y, bounds.size.w - 16, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  y += 22 + gap;

  // Best Day
  graphics_draw_text(ctx, "Best Day", FONT_CAPTION,
                     GRect(8, y, bounds.size.w - 16, 16),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  y += 16;
  char best_text[16];
  snprintf(best_text, sizeof(best_text), "%d ml", best_day);
  graphics_draw_text(ctx, best_text, FONT_BODY,
                     GRect(8, y, bounds.size.w - 16, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  y += 22 + gap;

  // Current Streak
  graphics_draw_text(ctx, "Current Streak", FONT_CAPTION,
                     GRect(8, y, bounds.size.w - 16, 16),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  y += 16;
  char streak_text[16];
  snprintf(streak_text, sizeof(streak_text), "%d days", streak);
  graphics_draw_text(ctx, streak_text, FONT_BODY,
                     GRect(8, y, bounds.size.w - 16, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Total Logged Days (smaller, at bottom)
  char total_text[32];
  snprintf(total_text, sizeof(total_text), "Total: %d days logged", logged_days);
  graphics_context_set_text_color(ctx, UI_MUTED);
  graphics_draw_text(ctx, total_text, FONT_CAPTION,
                     GRect(8, bounds.size.h - 24, bounds.size.w - 16, 18),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  graphics_context_set_text_color(ctx, UI_TEXT);
}
