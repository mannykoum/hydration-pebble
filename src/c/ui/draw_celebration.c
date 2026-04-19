#include "ui.h"

void draw_celebration(GContext *ctx, GRect bounds, UIState *ui_state) {
  graphics_context_set_fill_color(ctx, UI_BG);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  
  graphics_context_set_text_color(ctx, UI_POSITIVE);
  graphics_draw_text(ctx,
    "GOAL MET!",
    FONT_DISPLAY,
    GRect(0, bounds.size.h / 2 - 20, bounds.size.w, 32),
    GTextOverflowModeTrailingEllipsis,
    GTextAlignmentCenter,
    NULL);
  
  int offset = ui_state->anim_on ? 0 : 3;
  
  int confetti_positions[][2] = {
    {20, 30 + offset}, {60, 15 + offset}, {100, 25 + offset},
    {25, 50 + offset}, {80, 45 + offset}, {120, 55 + offset},
    {15, 75 + offset}, {50, 80 + offset}, {90, 70 + offset}, {130, 75 + offset},
    {35, 100 + offset}, {70, 95 + offset}, {110, 105 + offset},
    {45, 120 + offset}, {95, 125 + offset}
  };
  
  for (int i = 0; i < 15; i++) {
    GColor color = (i % 2 == 0) ? UI_ACCENT : UI_POSITIVE;
    graphics_context_set_fill_color(ctx, color);
    
    if (i % 3 == 0) {
      graphics_fill_rect(ctx, GRect(confetti_positions[i][0], confetti_positions[i][1], 4, 4), 4, GCornersAll);
    } else {
      graphics_fill_circle(ctx, GPoint(confetti_positions[i][0], confetti_positions[i][1]), 2);
    }
  }
}
