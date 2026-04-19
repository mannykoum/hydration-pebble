#include "ui.h"
#include "../utils/format.h"
#include <stdio.h>

void draw_amount_view(GContext *ctx, GRect bounds, UIState *ui_state) {
  DayData *today = ensure_today_day(ui_state->state);
  int goal_met = today->total_ml >= ui_state->state->goal_ml;

  // Blocky cup icon with shake animation
  int cup_x = 6;
  int cup_y = 14;
  int cup_width = 24;
  int cup_height = 40;
  
  // Shake animation when amount is selected
  if (ui_state->anim_on && ui_state->view == VIEW_AMOUNT) {
    for (int i = 0; i < MAX_AMOUNTS; i++) {
      if (i == ui_state->selected_amount) {
        cup_y = 14 + i * 24;
        cup_x += (i % 2 == 0) ? 3 : -3;
        break;
      }
    }
  }
  
  // Cup rim (top edge)
  graphics_context_set_stroke_color(ctx, UI_TEXT);
  graphics_draw_line(ctx, GPoint(cup_x, cup_y), GPoint(cup_x + cup_width, cup_y));
  graphics_draw_line(ctx, GPoint(cup_x, cup_y + 1), GPoint(cup_x + cup_width, cup_y + 1));
  
  // Cup body (tapered sides)
  graphics_draw_round_rect(ctx, GRect(cup_x + 2, cup_y + 2, cup_width - 4, cup_height - 8), 4);
  graphics_draw_line(ctx, GPoint(cup_x, cup_y), GPoint(cup_x + 2, cup_y + cup_height - 6));
  graphics_draw_line(ctx, GPoint(cup_x + cup_width, cup_y), GPoint(cup_x + cup_width - 2, cup_y + cup_height - 6));
  
  // Cup base
  graphics_draw_line(ctx, GPoint(cup_x + 2, cup_y + cup_height - 6), GPoint(cup_x + cup_width - 2, cup_y + cup_height - 6));
  graphics_draw_line(ctx, GPoint(cup_x + 2, cup_y + cup_height - 5), GPoint(cup_x + cup_width - 2, cup_y + cup_height - 5));
  
  // Water level indicator (fills 60% of cup)
  int water_height = (cup_height - 10) * 6 / 10;
  graphics_context_set_fill_color(ctx, UI_ACCENT);
  graphics_fill_rect(ctx, GRect(cup_x + 4, cup_y + cup_height - 8 - water_height, cup_width - 8, water_height), 4, GCornersAll);

  for (int i = 0; i < MAX_AMOUNTS; i++) {
    char line[20];
    format_amount(ui_state->state, ui_state->state->amounts_ml[i], line, sizeof(line));
    GRect row = GRect(34, 12 + i * 24, bounds.size.w - 40, 22);
    if (i == ui_state->selected_amount) {
      graphics_context_set_fill_color(ctx, ui_state->anim_on ? UI_ACCENT_ALT : UI_ACCENT);
      graphics_fill_rect(ctx, row, 4, GCornersAll);
      graphics_context_set_text_color(ctx, GColorWhite);
    } else {
      graphics_context_set_text_color(ctx, UI_TEXT);
    }

    graphics_draw_text(ctx, line, FONT_BODY, row,
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
  graphics_context_set_text_color(ctx, UI_TEXT);

  if (goal_met) {
    graphics_context_set_text_color(ctx, UI_POSITIVE);
    graphics_draw_text(ctx,
      ui_state->anim_on ? "Goal met!" : "Great job!",
      FONT_CAPTION,
      GRect(0, bounds.size.h - 32, bounds.size.w, 30),
      GTextOverflowModeTrailingEllipsis,
      GTextAlignmentCenter,
      NULL);
    graphics_context_set_text_color(ctx, UI_TEXT);
  } else {
    graphics_draw_text(ctx,
      ui_state->edit_amount ? "UP/DOWN: edit amount" : "SELECT: add/remove\nHold: edit amount",
      FONT_CAPTION,
      GRect(0, bounds.size.h - 32, bounds.size.w, 30),
      GTextOverflowModeWordWrap,
      GTextAlignmentCenter,
      NULL);
  }
}
