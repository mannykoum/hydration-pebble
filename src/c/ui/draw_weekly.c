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

  for (int i = 0; i < 7; i++) {
    int offset = 6 - i;
    int total = day_total(ui_state->state, offset);
    int fill_h = (total * 50) / goal;
    if (fill_h > 50) {
      fill_h = 50;
    }
    
    int bar_x = h_margin + i * bar_w + (bar_spacing / 2);
    GRect bar = GRect(bar_x, 26, actual_bar_w, 50);
    
    graphics_context_set_stroke_color(ctx, UI_TEXT);
    graphics_draw_round_rect(ctx, bar, 4);
    
    if (fill_h > 0) {
      graphics_context_set_fill_color(ctx, UI_ACCENT);
      for (int grad = 0; grad < fill_h; grad += 2) {
        int remaining = fill_h - grad;
        int chunk = remaining < 2 ? remaining : 2;
        graphics_fill_rect(ctx, 
          GRect(bar.origin.x + 1, bar.origin.y + bar.size.h - grad - chunk, 
                bar.size.w - 2, chunk), 4, GCornersAll);
      }
    }
    
    graphics_context_set_text_color(ctx, UI_TEXT);
    graphics_draw_text(ctx, days[i], FONT_CAPTION,
                       GRect(bar_x, 78, actual_bar_w, 18), 
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
  }

  graphics_context_set_text_color(ctx, UI_TEXT);
  graphics_draw_text(ctx, "Last 7 days", FONT_BODY,
                     GRect(0, 2, bounds.size.w, 20), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
}
