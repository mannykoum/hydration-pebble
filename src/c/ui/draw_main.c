#include "ui.h"
#include "../utils/format.h"
#include <stdio.h>
#include <string.h>

void draw_main_view(GContext *ctx, GRect bounds, UIState *ui_state) {
  DayData *today = ui_state->today;
  int total = today->total_ml;
  int goal = ui_state->state->goal_ml > 0 ? ui_state->state->goal_ml : 2800;

  // Top bar: day progress left, intake right
  time_t now = time(NULL);
  struct tm *tm_now = localtime(&now);
  int minutes = tm_now->tm_hour * 60 + tm_now->tm_min;
  int day_progress = (minutes * 100) / (24 * 60);
  char day_text[32];
  snprintf(day_text, sizeof(day_text), "Day:%d%%", day_progress);
  graphics_context_set_text_color(ctx, UI_TEXT);
  graphics_draw_text(ctx, day_text, FONT_CAPTION,
    GRect(8, 4, 70, 18),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  char amount_text[32];
  format_amount(ui_state->state, total, amount_text, sizeof(amount_text));
  graphics_draw_text(ctx, amount_text, FONT_CAPTION,
    GRect(bounds.size.w / 2, 4, bounds.size.w / 2 - 8, 18),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);

  // Centered progress ring
  int ring_radius = 45;
  int ring_stroke = 8;
  GPoint ring_center = GPoint(bounds.size.w / 2, bounds.size.h / 2 - 10);

  int progress_pct = (total * 100) / (goal > 0 ? goal : 1);
  if (progress_pct > 100) progress_pct = 100;
  int32_t angle_end = (TRIG_MAX_ANGLE * progress_pct) / 100;

  // Draw unfilled ring background track
  GRect outer_rect = GRect(ring_center.x - ring_radius, ring_center.y - ring_radius,
                           ring_radius * 2, ring_radius * 2 );
  graphics_context_set_fill_color(ctx, UI_MUTED);
  graphics_fill_radial(ctx, outer_rect, GOvalScaleModeFitCircle, ring_stroke, 0, TRIG_MAX_ANGLE);

  // Draw filled portion
  if (progress_pct > 0) {
    graphics_context_set_fill_color(ctx, UI_ACCENT);
    graphics_fill_radial(ctx, outer_rect, GOvalScaleModeFitCircle, ring_stroke, 0, angle_end);
  }

  // Draw border/outline around the ring
  graphics_context_set_stroke_color(ctx, UI_ACCENT);
  graphics_draw_circle(ctx, ring_center, ring_radius);
  graphics_draw_circle(ctx, ring_center, ring_radius - ring_stroke);

  // Percentage text inside ring
  char progress_text[16];
  snprintf(progress_text, sizeof(progress_text), "%d%%", progress_pct);
  graphics_context_set_text_color(ctx, UI_TEXT);
  graphics_draw_text(ctx, progress_text, FONT_DISPLAY,
    GRect(ring_center.x - 40, ring_center.y - 18, 80, 36),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Streak badge below ring, left side
  if (ui_state->state->current_streak > 0) {
    int badge_x = 16;
    int badge_y = ring_center.y + ring_radius + 4;

    graphics_context_set_fill_color(ctx, UI_POSITIVE);

    GPoint flame_points[3];
    flame_points[0] = GPoint(badge_x + 8, badge_y + 18);
    flame_points[1] = GPoint(badge_x + 16, badge_y + 18);
    flame_points[2] = GPoint(badge_x + 12, badge_y + 4);
    GPathInfo flame_path_info = {
      .num_points = 3,
      .points = flame_points
    };
    GPath *flame_path = gpath_create(&flame_path_info);
    gpath_draw_filled(ctx, flame_path);
    gpath_destroy(flame_path);

    graphics_fill_circle(ctx, GPoint(badge_x + 12, badge_y + 6), 3);
    graphics_fill_circle(ctx, GPoint(badge_x + 10, badge_y + 10), 2);
    graphics_fill_circle(ctx, GPoint(badge_x + 14, badge_y + 10), 2);

    char streak_text[8];
    snprintf(streak_text, sizeof(streak_text), "%d", ui_state->state->current_streak);
    graphics_context_set_text_color(ctx, UI_POSITIVE);
    graphics_draw_text(ctx, streak_text, FONT_CAPTION,
      GRect(badge_x - 4, badge_y + 16, 32, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    graphics_context_set_text_color(ctx, UI_TEXT);
  }

  // Instructions at bottom
  graphics_context_set_text_color(ctx, UI_MUTED);
  graphics_draw_text(ctx,
    ui_state->edit_goal ? "Goal edit: use UP/DOWN" : "Hold SELECT to edit goal",
    FONT_CAPTION,
    GRect(8, bounds.size.h - 20, bounds.size.w - 16, 14),
    GTextOverflowModeTrailingEllipsis,
    GTextAlignmentCenter,
    NULL);
  graphics_context_set_text_color(ctx, UI_TEXT);
}
