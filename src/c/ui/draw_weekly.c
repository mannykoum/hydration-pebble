#include "ui.h"
#include <stdio.h>

void draw_weekly_view(GContext *ctx, GRect bounds, UIState *ui_state) {
  int goal = ui_state->state->goal_ml > 0 ? ui_state->state->goal_ml : 2800;
  
  int h_margin = 16;
  int chart_w = bounds.size.w - (h_margin * 2);
  int bar_w = chart_w / 7;
  int bar_spacing = 4;
  int actual_bar_w = bar_w - bar_spacing;
  
  const char* days[] = {"S", "M", "T", "W", "T", "F", "S"};

  int title_h = 22;
  int label_h = 18;
  int bar_h = 50;
  int content_h = bar_h + label_h;
  int top_y = title_h + (bounds.size.h - title_h - content_h) / 2;

  for (int i = 0; i < 7; i++) {
    int offset = 6 - i;
    int total = day_total(ui_state->state, offset);
    int fill_h = (total * bar_h) / goal;
    if (fill_h > bar_h - 2) {
      fill_h = bar_h - 2;
    }
    
    int bar_x = h_margin + i * bar_w + (bar_spacing / 2);
    GRect bar = GRect(bar_x, top_y, actual_bar_w, bar_h);
    
    graphics_context_set_stroke_color(ctx, UI_TEXT);
    graphics_draw_round_rect(ctx, bar, 4);
    
    if (fill_h > 0) {
      graphics_context_set_fill_color(ctx, UI_ACCENT);
      graphics_fill_rect(ctx, 
        GRect(bar.origin.x + 1, bar.origin.y + bar.size.h - 1 - fill_h, 
              bar.size.w - 2, fill_h), 0, GCornerNone);
    }
    
    graphics_context_set_text_color(ctx, UI_TEXT);
    graphics_draw_text(ctx, days[i], FONT_CAPTION,
                       GRect(bar_x, top_y + bar_h + 2, actual_bar_w, label_h), 
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
  }

  graphics_context_set_text_color(ctx, UI_TEXT);
  graphics_draw_text(ctx, "Last 7 days", FONT_BODY,
                     GRect(0, 2, bounds.size.w, 20), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
}
