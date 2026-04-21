#include "../utils/format.h"
#include "ui.h"
#include <stdio.h>

void draw_amount_view(GContext *ctx, GRect bounds, UIState *ui_state) {
  DayData *today = ui_state->today;
  int goal_met = today->total_ml >= ui_state->state->goal_ml;

  // Symmetric cup icon (fixed position, no animation)
  int cup_x = 5;
  int cup_y = 56;
  int cw = 20;
  int ch = 36;

  graphics_context_set_stroke_color(ctx, UI_TEXT);
  // Cup body
  graphics_draw_rect(ctx, GRect(cup_x, cup_y, cw, ch));
  // Rim - slightly wider
  graphics_draw_line(ctx, GPoint(cup_x - 1, cup_y), GPoint(cup_x + cw, cup_y));
  // Base - slightly wider
  graphics_draw_line(ctx, GPoint(cup_x - 1, cup_y + ch),
                     GPoint(cup_x + cw, cup_y + ch));
  // Handle on right
  graphics_draw_rect(ctx, GRect(cup_x + cw, cup_y + 8, 6, 14));

  // Water fill based on progress
  int goal = ui_state->state->goal_ml > 0 ? ui_state->state->goal_ml : 2800;
  int fill_pct = (today->total_ml * 100) / goal;
  if (fill_pct > 100)
    fill_pct = 100;
  int water_h = (ch - 2) * fill_pct / 100;
  if (water_h > 0) {
    graphics_context_set_fill_color(ctx, UI_ACCENT);
    graphics_fill_rect(
        ctx, GRect(cup_x + 1, cup_y + ch - 1 - water_h, cw - 2, water_h), 0,
        GCornerNone);
  }

  for (int i = 0; i < MAX_AMOUNTS; i++) {
    char line[20];
    format_amount(ui_state->state, ui_state->state->amounts_ml[i], line,
                  sizeof(line));
    GRect row = GRect(34, 10 + i * 20, bounds.size.w - 40, 20);
    if (i == ui_state->selected_amount) {
      graphics_context_set_fill_color(ctx, UI_ACCENT);
      graphics_fill_rect(ctx, row, 4, GCornersAll);
      graphics_context_set_text_color(ctx, GColorWhite);
    } else {
      graphics_context_set_text_color(ctx, UI_TEXT);
    }

    graphics_draw_text(ctx, line, FONT_BODY, row,
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft,
                       NULL);
  }
  graphics_context_set_text_color(ctx, UI_TEXT);

  if (goal_met) {
    graphics_context_set_text_color(ctx, UI_POSITIVE);
    graphics_draw_text(
        ctx, ui_state->anim_on ? "Goal met!" : "Great job!", FONT_CAPTION,
        GRect(0, bounds.size.h - 34, bounds.size.w, 32),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    graphics_context_set_text_color(ctx, UI_TEXT);
  } else {
    graphics_context_set_text_color(ctx, UI_MUTED);
    graphics_draw_text(
        ctx,
        ui_state->edit_amount ? "UP/DOWN: edit amount"
                              : "SELECT: add/remove\nHold: edit amount",
        FONT_CAPTION,
        GRect(8, bounds.size.h - 34, bounds.size.w - 16, 14),
        GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  }
}
