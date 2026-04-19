#include "ui.h"
#include <stdio.h>

void draw_detail_view(GContext *ctx, GRect bounds, UIState *ui_state) {
  DayData *day = day_by_offset(ui_state->state, ui_state->selected_day_offset);
  
  char heading[32];
  if (ui_state->selected_day_offset == 0) {
    snprintf(heading, sizeof(heading), "Today");
  } else {
    snprintf(heading, sizeof(heading), "%d day%s ago", ui_state->selected_day_offset,
             ui_state->selected_day_offset == 1 ? "" : "s");
  }
  graphics_draw_text(ctx, heading, FONT_CAPTION,
    GRect(8, 2, bounds.size.w - 16, 16),
    GTextOverflowModeTrailingEllipsis,
    GTextAlignmentCenter,
    NULL);

  if (!day) {
    graphics_context_set_text_color(ctx, UI_MUTED);
    graphics_draw_text(ctx, "No data", FONT_BODY,
      GRect(0, bounds.size.h / 2 - 9, bounds.size.w, 18),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    graphics_draw_text(ctx,
      ui_state->selecting_day ? "Up/down choose day" : "SELECT to choose day",
      FONT_CAPTION,
      GRect(8, bounds.size.h - 20, bounds.size.w - 16, 14),
      GTextOverflowModeTrailingEllipsis,
      GTextAlignmentCenter,
      NULL);
    return;
  }

  int goal = ui_state->state->goal_ml;
  int max_value = goal;
  if (day->total_ml > max_value) {
    max_value = day->total_ml;
  }
  if (max_value < 1) {
    max_value = 1;
  }

  GRect plot = GRect(8, 20, bounds.size.w - 16, bounds.size.h - 40);
  graphics_draw_rect(ctx, plot);

  graphics_context_set_stroke_color(ctx, UI_MUTED);
  for (int i = 1; i <= 3; i++) {
    int y = plot.origin.y + (plot.size.h * i) / 4;
    graphics_draw_line(ctx, GPoint(plot.origin.x + 1, y), GPoint(plot.origin.x + plot.size.w - 1, y));

    char label[12];
    int mark = max_value - (max_value * i / 4);
    snprintf(label, sizeof(label), "%d", mark);
    graphics_context_set_text_color(ctx, UI_TEXT);
    graphics_draw_text(ctx, label, FONT_CAPTION,
      GRect(plot.origin.x + 2, y - 10, 30, 14), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }

  if (day->point_count > 0) {
    #ifdef PBL_COLOR
    graphics_context_set_stroke_color(ctx, GColorCobaltBlue);
    #else
    graphics_context_set_stroke_color(ctx, UI_MUTED);
    #endif
    GPoint last = GPoint(plot.origin.x + 1, plot.origin.y + plot.size.h - 1);
    for (uint8_t i = 0; i < day->point_count; i++) {
      int x = plot.origin.x + (day->minutes[i] * plot.size.w) / (24 * 60);
      int y = plot.origin.y + plot.size.h -
              (int)(((int64_t)day->cumulative_ml[i] * plot.size.h) / max_value);
      if (y < plot.origin.y) {
        y = plot.origin.y;
      }
      GPoint point = GPoint(x, y);
      graphics_draw_line(ctx, last, point);
      graphics_draw_line(ctx, GPoint(last.x, last.y + 1), GPoint(point.x, point.y + 1));
      graphics_draw_line(ctx, GPoint(last.x + 1, last.y), GPoint(point.x + 1, point.y));
      last = point;
    }
  }

  graphics_context_set_text_color(ctx, UI_MUTED);
  graphics_draw_text(ctx,
    ui_state->selecting_day ? "Up/down choose day" : "SELECT to choose day",
    FONT_CAPTION,
    GRect(8, bounds.size.h - 20, bounds.size.w - 16, 14),
    GTextOverflowModeTrailingEllipsis,
    GTextAlignmentCenter,
    NULL);
}
