#include "ui.h"

void draw_celebration(GContext *ctx, GRect bounds, UIState *ui_state) {
  // Inset to preserve screen border
  GRect inner = grect_inset(bounds, GEdgeInsets(3));
  graphics_context_set_fill_color(ctx, UI_BG);
  graphics_fill_rect(ctx, inner, 0, GCornerNone);
  
  graphics_context_set_text_color(ctx, UI_POSITIVE);
  graphics_draw_text(ctx,
    "GOAL MET!",
    FONT_DISPLAY,
    GRect(inner.origin.x, inner.origin.y + inner.size.h / 2 - 20, inner.size.w, 32),
    GTextOverflowModeTrailingEllipsis,
    GTextAlignmentCenter,
    NULL);
  
#ifdef PBL_COLOR
  // Generate confetti positions relative to bounds
  int cw = inner.size.w;
  int ch = inner.size.h;
  int cx = inner.origin.x;
  int cy = inner.origin.y;
  int offset = ui_state->anim_on ? 0 : 3;
  
  // Spread confetti across the screen using proportional positions
  int confetti_positions[][2] = {
    {cw * 14 / 100, ch * 18 / 100 + offset},
    {cw * 42 / 100, ch * 9 / 100 + offset},
    {cw * 69 / 100, ch * 15 / 100 + offset},
    {cw * 17 / 100, ch * 30 / 100 + offset},
    {cw * 56 / 100, ch * 27 / 100 + offset},
    {cw * 83 / 100, ch * 33 / 100 + offset},
    {cw * 10 / 100, ch * 45 / 100 + offset},
    {cw * 35 / 100, ch * 48 / 100 + offset},
    {cw * 63 / 100, ch * 42 / 100 + offset},
    {cw * 90 / 100, ch * 45 / 100 + offset},
    {cw * 24 / 100, ch * 60 / 100 + offset},
    {cw * 49 / 100, ch * 57 / 100 + offset},
    {cw * 76 / 100, ch * 63 / 100 + offset},
    {cw * 31 / 100, ch * 71 / 100 + offset},
    {cw * 66 / 100, ch * 74 / 100 + offset}
  };
  
  for (int i = 0; i < 15; i++) {
    int px = cx + confetti_positions[i][0];
    int py = cy + confetti_positions[i][1];
    
    GColor color = (i % 2 == 0) ? UI_ACCENT : UI_POSITIVE;
    graphics_context_set_fill_color(ctx, color);
    
    if (i % 3 == 0) {
      graphics_fill_rect(ctx, GRect(px, py, 4, 4), 4, GCornersAll);
    } else {
      graphics_fill_circle(ctx, GPoint(px, py), 2);
    }
  }
#endif
}
