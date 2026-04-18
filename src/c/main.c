#include <pebble.h>
#include <stdint.h>
#include <stdlib.h>

#define STORAGE_KEY_STATE 1
#define MAX_AMOUNTS 4
#define MAX_DAYS 14
#define MAX_POINTS 32
#define REPEAT_STREAK_DIVISOR 3
#define REPEAT_STEP_BASE 10
#define REPEAT_STEP_INCREMENT 15
#define KEY_LOG_DELTA_ML 6
#define KEY_LOG_TOTAL_ML 7
#define KEY_LOG_MINUTE 8

typedef enum {
  UNIT_ML = 0,
  UNIT_CUPS = 1,
  UNIT_PINTS = 2
} Unit;

typedef enum {
  VIEW_MAIN = 0,
  VIEW_AMOUNT = 1,
  VIEW_DETAIL = 2,
  VIEW_WEEKLY = 3,
  VIEW_STATS = 4,
  VIEW_COUNT
} MainView;

typedef struct {
  int date_key;
  int total_ml;
  uint8_t point_count;
  uint16_t minutes[MAX_POINTS];
  uint16_t cumulative_ml[MAX_POINTS];
} DayData;

typedef struct {
  int goal_ml;
  int amounts_ml[MAX_AMOUNTS];
  uint8_t unit;
  int current_streak;
  DayData days[MAX_DAYS];
} PersistedState;

static PersistedState s_state;
static Window *s_main_window;
static Layer *s_canvas_layer;
static MainView s_view = VIEW_MAIN;
static bool s_edit_goal = false;
static bool s_edit_amount = false;
static bool s_selecting_day = false;
static int s_selected_amount = 0;
static int s_selected_day_offset = 0;
static int s_repeat_streak = 0;
static int s_last_repeat_direction = 0;
static bool s_anim_on = false;
static bool s_celebrating = false;
static int s_celebration_counter = 0;
static time_t s_last_intake_time = 0;
static int s_last_intake_amount = 0;
static uint8_t s_last_point_count = 0;
static uint8_t s_milestones_hit = 0;
static bool s_show_undo_message = false;
static AppTimer *s_undo_message_timer = NULL;

static const int APP_KEYS[] = {0, 1, 2, 3, 4, 5};

#ifdef PBL_COLOR
static const GColor UI_BG = GColorWhite;
static const GColor UI_TEXT = GColorBlack;
static const GColor UI_MUTED = GColorDarkGray;
static const GColor UI_ACCENT = GColorCyan;
static const GColor UI_ACCENT_ALT = GColorBlue;
static const GColor UI_POSITIVE = GColorGreen;
#else
static const GColor UI_BG = GColorWhite;
static const GColor UI_TEXT = GColorBlack;
static const GColor UI_MUTED = GColorBlack;
static const GColor UI_ACCENT = GColorBlack;
static const GColor UI_ACCENT_ALT = GColorBlack;
static const GColor UI_POSITIVE = GColorBlack;
#endif

// Typography scale
#define FONT_DISPLAY fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD)
#define FONT_TITLE fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD)
#define FONT_BODY fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD)
#define FONT_CAPTION fonts_get_system_font(FONT_KEY_GOTHIC_14)
#define FONT_SMALL fonts_get_system_font(FONT_KEY_GOTHIC_09)

static int day_key_from_time(time_t t) {
  struct tm *tm_now = localtime(&t);
  return (tm_now->tm_year + 1900) * 10000 + (tm_now->tm_mon + 1) * 100 + tm_now->tm_mday;
}

static int current_minutes(void) {
  time_t now = time(NULL);
  struct tm *tm_now = localtime(&now);
  return tm_now->tm_hour * 60 + tm_now->tm_min;
}

static int unit_multiplier(Unit unit) {
  switch(unit) {
    case UNIT_CUPS: return 240;
    case UNIT_PINTS: return 473;
    case UNIT_ML:
    default: return 1;
  }
}

static void format_amount(int ml, char *buffer, size_t size) {
  int mul = unit_multiplier((Unit)s_state.unit);
  int abs_ml = abs(ml);
  int whole = abs_ml / mul;
  int frac = (abs_ml % mul) * 10 / mul;
  char sign = ml < 0 ? '-' : '+';
  const char *suffix = s_state.unit == UNIT_ML ? "ml" : (s_state.unit == UNIT_CUPS ? "c" : "pt");
  if (s_state.unit == UNIT_ML) {
    snprintf(buffer, size, "%+d%s", ml, suffix);
  } else {
    if (frac == 0) {
      snprintf(buffer, size, "%c%d%s", sign, whole, suffix);
    } else {
      snprintf(buffer, size, "%c%d.%d%s", sign, whole, frac, suffix);
    }
  }
}

static void save_state(void) {
  persist_write_data(STORAGE_KEY_STATE, &s_state, sizeof(s_state));
}

static int find_day_index(int key) {
  for (int i = 0; i < MAX_DAYS; i++) {
    if (s_state.days[i].date_key == key) {
      return i;
    }
  }
  return -1;
}

static int oldest_day_index(void) {
  int oldest = -1;
  for (int i = 0; i < MAX_DAYS; i++) {
    if (s_state.days[i].date_key == 0) {
      return i;
    }
    if (oldest < 0 || s_state.days[i].date_key < s_state.days[oldest].date_key) {
      oldest = i;
    }
  }
  return oldest;
}

static DayData *ensure_today_day(void) {
  time_t now = time(NULL);
  int key = day_key_from_time(now);
  int idx = find_day_index(key);
  if (idx >= 0) {
    return &s_state.days[idx];
  }

  // Check if yesterday met goal to update streak
  time_t yesterday_time = now - 24 * 60 * 60;
  int yesterday_key = day_key_from_time(yesterday_time);
  int yesterday_idx = find_day_index(yesterday_key);
  int goal = s_state.goal_ml > 0 ? s_state.goal_ml : 2800;
  
  if (yesterday_idx >= 0 && s_state.days[yesterday_idx].total_ml >= goal) {
    s_state.current_streak++;
  } else {
    s_state.current_streak = 0;
  }

  idx = oldest_day_index();
  memset(&s_state.days[idx], 0, sizeof(DayData));
  s_state.days[idx].date_key = key;
  return &s_state.days[idx];
}

static void reset_if_new_day(void) {
  (void)ensure_today_day();
  s_milestones_hit = 0;
}

static DayData *day_by_offset(int offset) {
  time_t now = time(NULL) - (time_t)offset * 24 * 60 * 60;
  int key = day_key_from_time(now);
  int idx = find_day_index(key);
  if (idx < 0) {
    return NULL;
  }
  return &s_state.days[idx];
}

static int day_total(int offset) {
  DayData *day = day_by_offset(offset);
  return day ? day->total_ml : 0;
}

static int calculate_weekly_avg(void) {
  int sum = 0;
  int count = 0;
  for (int i = 0; i < 7; i++) {
    int total = day_total(i);
    if (total > 0) {
      sum += total;
      count++;
    }
  }
  return count > 0 ? sum / count : 0;
}

static int find_best_day(void) {
  int best = 0;
  for (int i = 0; i < MAX_DAYS; i++) {
    if (s_state.days[i].total_ml > best) {
      best = s_state.days[i].total_ml;
    }
  }
  return best;
}

static int count_logged_days(void) {
  int count = 0;
  for (int i = 0; i < MAX_DAYS; i++) {
    if (s_state.days[i].total_ml > 0) {
      count++;
    }
  }
  return count;
}

static void send_log_event(int delta_ml, int total_ml, int minute) {
  DictionaryIterator *iter = NULL;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK || !iter) {
    return;
  }
  dict_write_int32(iter, KEY_LOG_DELTA_ML, delta_ml);
  dict_write_int32(iter, KEY_LOG_TOTAL_ML, total_ml);
  dict_write_int16(iter, KEY_LOG_MINUTE, (int16_t)minute);
  app_message_outbox_send();
}

static void add_intake(int delta_ml) {
  DayData *today = ensure_today_day();
  
  // Store for undo functionality
  s_last_intake_time = time(NULL);
  s_last_intake_amount = delta_ml;
  
  int next_total = today->total_ml + delta_ml;
  if (next_total < 0) {
    next_total = 0;
  }
  today->total_ml = next_total;

  int minute = current_minutes();
  if (today->point_count == 0) {
    today->minutes[0] = (uint16_t)minute;
    today->cumulative_ml[0] = (uint16_t)today->total_ml;
    today->point_count = 1;
  } else if (today->point_count < MAX_POINTS) {
    today->minutes[today->point_count] = (uint16_t)minute;
    today->cumulative_ml[today->point_count] = (uint16_t)today->total_ml;
    today->point_count++;
  } else {
    today->minutes[MAX_POINTS - 1] = (uint16_t)minute;
    today->cumulative_ml[MAX_POINTS - 1] = (uint16_t)today->total_ml;
  }

  save_state();
  send_log_event(delta_ml, today->total_ml, minute);
  
  // Vibration feedback
  vibes_short_pulse();
  
  // Check for milestone notifications
  int goal = s_state.goal_ml > 0 ? s_state.goal_ml : 2800;
  int progress_percent = (today->total_ml * 100) / goal;
  
  if (progress_percent >= 50 && !(s_milestones_hit & (1 << 0))) {
    s_milestones_hit |= (1 << 0);
    vibes_long_pulse();
  }
  if (progress_percent >= 75 && !(s_milestones_hit & (1 << 1))) {
    s_milestones_hit |= (1 << 1);
    vibes_long_pulse();
  }
  if (progress_percent >= 100 && !(s_milestones_hit & (1 << 2))) {
    s_milestones_hit |= (1 << 2);
    vibes_long_pulse();
  }
}

static int progress_step(void) {
  int streak_factor = s_repeat_streak / REPEAT_STREAK_DIVISOR;
  if (streak_factor > 10) {
    streak_factor = 10;
  }
  return REPEAT_STEP_BASE + streak_factor * REPEAT_STEP_INCREMENT;
}

static void move_view(MainView new_view) {
  s_view = new_view;
  s_edit_goal = false;
  s_edit_amount = false;
  s_selecting_day = false;
  s_repeat_streak = 0;
  vibes_short_pulse();
  layer_mark_dirty(s_canvas_layer);
}

static void draw_progress_bar(GContext *ctx, GRect frame, int numerator, int denominator, const char *label) {
  graphics_context_set_stroke_color(ctx, UI_MUTED);
  graphics_draw_round_rect(ctx, frame, 4);

  int fill_width = 0;
  if (denominator > 0 && numerator > 0) {
    int ratio = (int)(((int64_t)numerator * frame.size.w) / denominator);
    fill_width = ratio > frame.size.w ? frame.size.w : ratio;
  }

  if (fill_width > 0) {
    graphics_context_set_fill_color(ctx, s_anim_on ? UI_ACCENT_ALT : UI_ACCENT);
    graphics_fill_rect(ctx, GRect(frame.origin.x + 1, frame.origin.y + 1, fill_width - 1, frame.size.h - 1), 4, GCornersAll);
  }

  graphics_context_set_text_color(ctx, UI_MUTED);
  graphics_draw_text(ctx, label, FONT_CAPTION,
    GRect(frame.origin.x, frame.origin.y - 16, frame.size.w, 14),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_context_set_text_color(ctx, UI_TEXT);
}

static void draw_main_view(GContext *ctx, GRect bounds) {
  DayData *today = ensure_today_day();
  int total = today->total_ml;
  int goal = s_state.goal_ml > 0 ? s_state.goal_ml : 2800;

  // Water drop graphic at top center
  graphics_context_set_fill_color(ctx, UI_ACCENT);
  int drop_center_x = bounds.size.w / 2;
  graphics_fill_circle(ctx, GPoint(drop_center_x, 16), 8);
  graphics_fill_circle(ctx, GPoint(drop_center_x - 1, 13), 4);
  graphics_fill_circle(ctx, GPoint(drop_center_x + 1, 13), 4);

  // Circular progress ring (dev: rounded corners with graphics_fill_radial)
  int ring_radius = 45;
  int ring_stroke = 8;
  GPoint ring_center = GPoint(bounds.size.w / 2, 70);
  
  // Calculate progress percentage
  int progress_pct = (total * 100) / (goal > 0 ? goal : 1);
  if (progress_pct > 100) progress_pct = 100;
  int32_t angle_end = (TRIG_MAX_ANGLE * progress_pct) / 100;
  
  // Draw ring by drawing multiple stroke circles
  graphics_context_set_stroke_color(ctx, UI_MUTED);
  for (int i = 0; i < ring_stroke; i++) {
    graphics_draw_circle(ctx, ring_center, ring_radius - i / 2);
  }
  
  // Draw filled portion - draw filled arc with rounded corners
  if (progress_pct > 0) {
    graphics_context_set_fill_color(ctx, s_anim_on ? UI_ACCENT_ALT : UI_ACCENT);
    GRect outer_rect = GRect(ring_center.x - ring_radius, ring_center.y - ring_radius, 
                             ring_radius * 2, ring_radius * 2);
    graphics_fill_radial(ctx, outer_rect, GOvalScaleModeFitCircle, ring_stroke, 0, angle_end);
  }
  
  // Percentage text centered in ring (using typography constants)
  char progress_text[16];
  snprintf(progress_text, sizeof(progress_text), "%d%%", progress_pct);
  graphics_context_set_text_color(ctx, UI_TEXT);
  graphics_draw_text(ctx, progress_text, FONT_DISPLAY,
    GRect(ring_center.x - 40, ring_center.y - 18, 80, 36),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Amount text below ring (using typography constants)
  char amount_text[32];
  format_amount(total, amount_text, sizeof(amount_text));
  graphics_draw_text(ctx, amount_text, FONT_TITLE,
    GRect(0, ring_center.y + ring_radius + 4, bounds.size.w, 28),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Day progress percentage (using typography constants)
  time_t now = time(NULL);
  struct tm *tm_now = localtime(&now);
  int minutes = tm_now->tm_hour * 60 + tm_now->tm_min;
  int day_progress = (minutes * 100) / (24 * 60);
  char day_text[32];
  snprintf(day_text, sizeof(day_text), "Day: %d%%", day_progress);
  graphics_draw_text(ctx, day_text, FONT_BODY,
    GRect(0, 128, bounds.size.w, 22),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Streak badge (top right corner)
  if (s_state.current_streak > 0) {
    int badge_x = bounds.size.w - 36;
    int badge_y = 8;
    
    // Draw flame icon using graphics primitives
    graphics_context_set_fill_color(ctx, UI_POSITIVE);
    
    // Flame base (triangle)
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
    
    // Flame tip circles
    graphics_fill_circle(ctx, GPoint(badge_x + 12, badge_y + 6), 3);
    graphics_fill_circle(ctx, GPoint(badge_x + 10, badge_y + 10), 2);
    graphics_fill_circle(ctx, GPoint(badge_x + 14, badge_y + 10), 2);
    
    // Streak number
    char streak_text[8];
    snprintf(streak_text, sizeof(streak_text), "%d", s_state.current_streak);
    graphics_context_set_text_color(ctx, UI_POSITIVE);
    graphics_draw_text(ctx, streak_text, FONT_CAPTION,
      GRect(badge_x - 4, badge_y + 16, 32, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    graphics_context_set_text_color(ctx, UI_TEXT);
  }

  // Instructions at bottom
  graphics_context_set_text_color(ctx, UI_MUTED);
  graphics_draw_text(ctx,
    s_edit_goal ? "Goal edit: use UP/DOWN" : "Hold SELECT to edit goal",
    FONT_CAPTION,
    GRect(8, bounds.size.h - 38, bounds.size.w - 16, 32),
    GTextOverflowModeWordWrap,
    GTextAlignmentCenter,
    NULL);
  graphics_context_set_text_color(ctx, UI_TEXT);
}

static void draw_amount_view(GContext *ctx, GRect bounds) {
  DayData *today = ensure_today_day();
  int goal_met = today->total_ml >= s_state.goal_ml;

  // Blocky cup icon with shake animation
  int cup_x = 6;
  int cup_y = 14;
  int cup_width = 24;
  int cup_height = 40;
  
  // Shake animation when amount is selected
  if (s_anim_on && s_view == VIEW_AMOUNT) {
    for (int i = 0; i < MAX_AMOUNTS; i++) {
      if (i == s_selected_amount) {
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
    format_amount(s_state.amounts_ml[i], line, sizeof(line));
    GRect row = GRect(34, 12 + i * 24, bounds.size.w - 40, 22);
    if (i == s_selected_amount) {
      graphics_context_set_fill_color(ctx, s_anim_on ? UI_ACCENT_ALT : UI_ACCENT);
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
    if (!s_celebrating && s_celebration_counter == 0) {
      s_celebrating = true;
      s_celebration_counter = 1;
      vibes_double_pulse();
    }
    graphics_context_set_text_color(ctx, UI_POSITIVE);
    graphics_draw_text(ctx,
      s_anim_on ? "Goal met!" : "Great job!",
      FONT_CAPTION,
      GRect(0, bounds.size.h - 20, bounds.size.w, 18),
      GTextOverflowModeTrailingEllipsis,
      GTextAlignmentCenter,
      NULL);
    graphics_context_set_text_color(ctx, UI_TEXT);
  } else {
    graphics_draw_text(ctx,
      s_edit_amount ? "Amount edit: use UP/DOWN" : "SELECT: add/remove, Hold SELECT: edit",
      FONT_CAPTION,
      GRect(0, bounds.size.h - 20, bounds.size.w, 18),
      GTextOverflowModeTrailingEllipsis,
      GTextAlignmentCenter,
      NULL);
  }
}

static void draw_detail_view(GContext *ctx, GRect bounds) {
  DayData *day = day_by_offset(s_selected_day_offset);
  
  char heading[32];
  if (s_selected_day_offset == 0) {
    snprintf(heading, sizeof(heading), "Today");
  } else {
    snprintf(heading, sizeof(heading), "%d day%s ago", s_selected_day_offset,
             s_selected_day_offset == 1 ? "" : "s");
  }
  graphics_draw_text(ctx, heading, FONT_CAPTION,
    GRect(8, 0, bounds.size.w - 16, 16), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  if (!day) {
    graphics_draw_text(ctx, "No data", FONT_BODY,
      GRect(0, bounds.size.h / 2 - 9, bounds.size.w, 18),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    graphics_draw_text(ctx,
      s_selecting_day ? "Up/down choose day" : "Select to choose day",
      FONT_CAPTION,
      GRect(0, bounds.size.h - 16, bounds.size.w, 16),
      GTextOverflowModeTrailingEllipsis,
      GTextAlignmentCenter,
      NULL);
    return;
  }

  int goal = s_state.goal_ml;
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
    graphics_draw_line(ctx, GPoint(plot.origin.x, y), GPoint(plot.origin.x + plot.size.w, y));

    char label[12];
    int mark = max_value - (max_value * i / 4);
    snprintf(label, sizeof(label), "%d", mark);
    graphics_context_set_text_color(ctx, UI_MUTED);
    graphics_draw_text(ctx, label, FONT_CAPTION,
      GRect(plot.origin.x + 2, y - 10, 30, 14), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }

  if (day->point_count > 0) {
    graphics_context_set_stroke_color(ctx, UI_ACCENT);
    GPoint last = GPoint(plot.origin.x, plot.origin.y + plot.size.h);
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

  graphics_draw_text(ctx,
    s_selecting_day ? "Up/down choose day" : "Select to choose day",
    FONT_CAPTION,
    GRect(0, bounds.size.h - 16, bounds.size.w, 16),
    GTextOverflowModeTrailingEllipsis,
    GTextAlignmentCenter,
    NULL);
}

static void draw_weekly_view(GContext *ctx, GRect bounds) {
  int goal = s_state.goal_ml > 0 ? s_state.goal_ml : 2800;
  
  int h_margin = 16;
  int chart_w = bounds.size.w - (h_margin * 2);
  int bar_w = chart_w / 7;
  int bar_spacing = 4;
  int actual_bar_w = bar_w - bar_spacing;
  
  const char* days[] = {"S", "M", "T", "W", "T", "F", "S"};

  for (int i = 0; i < 7; i++) {
    int offset = 6 - i;
    int total = day_total(offset);
    int fill_h = (total * 50) / goal;
    if (fill_h > 50) {
      fill_h = 50;
    }
    
    int bar_x = h_margin + i * bar_w + (bar_spacing / 2);
    GRect bar = GRect(bar_x, 26, actual_bar_w, 50);
    
    graphics_context_set_stroke_color(ctx, UI_TEXT);
    graphics_draw_rect(ctx, bar);
    
    if (fill_h > 0) {
      graphics_context_set_fill_color(ctx, UI_ACCENT);
      for (int grad = 0; grad < fill_h; grad += 2) {
        int remaining = fill_h - grad;
        int chunk = remaining < 2 ? remaining : 2;
        graphics_fill_rect(ctx, 
          GRect(bar.origin.x + 1, bar.origin.y + bar.size.h - grad - chunk, 
                bar.size.w - 2, chunk), 0, GCornerNone);
      }
    }
    
    graphics_context_set_text_color(ctx, UI_TEXT);
    graphics_draw_text(ctx, days[i], fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(bar_x, 78, actual_bar_w, 18), 
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
  }

  graphics_context_set_text_color(ctx, UI_TEXT);
  graphics_draw_text(ctx, "Last 7 days", fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(0, 2, bounds.size.w, 20), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
}

static void draw_stats_view(GContext *ctx, GRect bounds) {
  int weekly_avg = calculate_weekly_avg();
  int best_day = find_best_day();
  int logged_days = count_logged_days();
  int streak = s_state.current_streak;
  
  graphics_context_set_text_color(ctx, UI_TEXT);
  
  // Title (using typography constants)
  graphics_draw_text(ctx, "Statistics", FONT_BODY,
                     GRect(0, 8, bounds.size.w, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  
  int y_offset = 36;
  int line_height = 32;
  
  // Weekly Average (using typography constants)
  graphics_draw_text(ctx, "Weekly Average:", FONT_BODY,
                     GRect(8, y_offset, bounds.size.w - 16, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  char avg_text[16];
  snprintf(avg_text, sizeof(avg_text), "%d ml", weekly_avg);
  graphics_draw_text(ctx, avg_text, FONT_TITLE,
                     GRect(8, y_offset + 20, bounds.size.w - 16, 28),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  
  y_offset += line_height + 16;
  
  // Best Day (using typography constants)
  graphics_draw_text(ctx, "Best Day:", FONT_BODY,
                     GRect(8, y_offset, bounds.size.w - 16, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  char best_text[16];
  snprintf(best_text, sizeof(best_text), "%d ml", best_day);
  graphics_draw_text(ctx, best_text, FONT_TITLE,
                     GRect(8, y_offset + 20, bounds.size.w - 16, 28),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  
  y_offset += line_height + 16;
  
  // Current Streak (using typography constants)
  graphics_draw_text(ctx, "Current Streak:", FONT_BODY,
                     GRect(8, y_offset, bounds.size.w - 16, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  char streak_text[16];
  snprintf(streak_text, sizeof(streak_text), "%d days", streak);
  graphics_draw_text(ctx, streak_text, FONT_TITLE,
                     GRect(8, y_offset + 20, bounds.size.w - 16, 28),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  
  // Total Logged Days (smaller, at bottom, using typography constants)
  char total_text[32];
  snprintf(total_text, sizeof(total_text), "Total: %d days logged", logged_days);
  graphics_context_set_text_color(ctx, UI_MUTED);
  graphics_draw_text(ctx, total_text, FONT_CAPTION,
                     GRect(8, bounds.size.h - 24, bounds.size.w - 16, 18),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  graphics_context_set_text_color(ctx, UI_TEXT);
}

static void draw_celebration(GContext *ctx, GRect bounds) {
  graphics_context_set_fill_color(ctx, UI_BG);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  
  graphics_context_set_text_color(ctx, UI_POSITIVE);
  graphics_draw_text(ctx,
    "GOAL MET!",
    fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
    GRect(0, bounds.size.h / 2 - 20, bounds.size.w, 32),
    GTextOverflowModeTrailingEllipsis,
    GTextAlignmentCenter,
    NULL);
  
  int offset = s_anim_on ? 0 : 3;
  
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
      graphics_fill_rect(ctx, GRect(confetti_positions[i][0], confetti_positions[i][1], 4, 4), 0, GCornerNone);
    } else {
      graphics_fill_circle(ctx, GPoint(confetti_positions[i][0], confetti_positions[i][1]), 2);
    }
  }
  
  s_celebration_counter++;
  if (s_celebration_counter > 8) {
    s_celebrating = false;
    s_celebration_counter = 0;
  }
}

static void canvas_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, UI_BG);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  graphics_context_set_text_color(ctx, UI_TEXT);

  if (s_celebrating) {
    draw_celebration(ctx, bounds);
    return;
  }

  switch (s_view) {
    case VIEW_MAIN: draw_main_view(ctx, bounds); break;
    case VIEW_AMOUNT: draw_amount_view(ctx, bounds); break;
    case VIEW_DETAIL: draw_detail_view(ctx, bounds); break;
    case VIEW_WEEKLY: draw_weekly_view(ctx, bounds); break;
    case VIEW_STATS: draw_stats_view(ctx, bounds); break;
    default: break;
  }
  
  // Draw undo message overlay if active
  if (s_show_undo_message) {
    GRect message_box = GRect(20, bounds.size.h / 2 - 20, bounds.size.w - 40, 40);
    graphics_context_set_fill_color(ctx, UI_ACCENT);
    graphics_fill_rect(ctx, message_box, 4, GCornersAll);
    graphics_context_set_stroke_color(ctx, UI_TEXT);
    graphics_draw_rect(ctx, message_box);
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, "Undone!", fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
      GRect(message_box.origin.x, message_box.origin.y + 8, message_box.size.w, 30),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    graphics_context_set_text_color(ctx, UI_TEXT);
  }
}

static void select_single_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_view == VIEW_AMOUNT && !s_edit_amount) {
    add_intake(s_state.amounts_ml[s_selected_amount]);
  } else if (s_view == VIEW_DETAIL && !s_selecting_day) {
    s_selecting_day = true;
  }
  layer_mark_dirty(s_canvas_layer);
}

static void select_long_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_view == VIEW_MAIN) {
    s_edit_goal = !s_edit_goal;
  } else if (s_view == VIEW_AMOUNT) {
    s_edit_amount = !s_edit_amount;
  }
  s_repeat_streak = 0;
  layer_mark_dirty(s_canvas_layer);
}

static void hide_undo_message(void *data) {
  s_show_undo_message = false;
  s_undo_message_timer = NULL;
  layer_mark_dirty(s_canvas_layer);
}

static void back_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_edit_goal || s_edit_amount || s_selecting_day) {
    s_edit_goal = false;
    s_edit_amount = false;
    s_selecting_day = false;
    save_state();
    layer_mark_dirty(s_canvas_layer);
  } else {
    // Check for undo functionality (within 10 seconds)
    time_t now = time(NULL);
    if (s_last_intake_amount != 0 && (now - s_last_intake_time) <= 10) {
      // Undo the last intake
      DayData *today = ensure_today_day();
      today->total_ml -= s_last_intake_amount;
      if (today->total_ml < 0) {
        today->total_ml = 0;
      }
      
      // Restore point_count to state before last intake
      today->point_count = s_last_point_count;
      
      save_state();
      
      // Clear undo data (only works once)
      s_last_intake_amount = 0;
      s_last_intake_time = 0;
      s_last_point_count = 0;
      
      // Show "Undone!" message
      s_show_undo_message = true;
      if (s_undo_message_timer) {
        app_timer_cancel(s_undo_message_timer);
      }
      s_undo_message_timer = app_timer_register(2000, hide_undo_message, NULL);
      
      vibes_short_pulse();
      layer_mark_dirty(s_canvas_layer);
    }
  }
}

static void apply_delta(int direction) {
  if (s_last_repeat_direction == direction) {
    s_repeat_streak++;
  } else {
    s_repeat_streak = 0;
    s_last_repeat_direction = direction;
  }

  if (s_edit_goal) {
    int delta = progress_step() * direction;
    s_state.goal_ml += delta;
    if (s_state.goal_ml < 100) {
      s_state.goal_ml = 100;
    }
    save_state();
  } else if (s_edit_amount) {
    int delta = progress_step() * direction;
    s_state.amounts_ml[s_selected_amount] += delta;
    save_state();
  } else if (s_selecting_day && s_view == VIEW_DETAIL) {
    int next = s_selected_day_offset - direction;
    if (next < 0) {
      next = 0;
    }
    if (next > MAX_DAYS - 1) {
      next = MAX_DAYS - 1;
    }
    s_selected_day_offset = next;
  } else if (s_view == VIEW_AMOUNT) {
    if (direction == 1) {
      /* up */
      if (s_selected_amount == 0) {
        move_view(VIEW_MAIN);
        return;
      }
      s_selected_amount--;
    } else {
      /* down — move to next row; do nothing when already at the last row */
      if (s_selected_amount < MAX_AMOUNTS - 1) {
        s_selected_amount++;
      }
    }
  } else if (s_view == VIEW_MAIN) {
    if (direction == 1) {
      move_view(VIEW_DETAIL);
    } else {
      s_selected_amount = 0;
      move_view(VIEW_AMOUNT);
    }
    return;
  } else if (s_view == VIEW_DETAIL) {
    if (direction == 1) {
      move_view(VIEW_WEEKLY);
    } else {
      move_view(VIEW_MAIN);
    }
    return;
  } else if (s_view == VIEW_WEEKLY) {
    if (direction == -1) {
      move_view(VIEW_DETAIL);
      return;
    }
    /* up from weekly: go to stats */
    move_view(VIEW_STATS);
    return;
  } else if (s_view == VIEW_STATS) {
    if (direction == -1) {
      move_view(VIEW_WEEKLY);
      return;
    }
    /* up from stats: do nothing */
  }

  layer_mark_dirty(s_canvas_layer);
}

static void up_handler(ClickRecognizerRef recognizer, void *context) {
  apply_delta(1);
}

static void down_handler(ClickRecognizerRef recognizer, void *context) {
  apply_delta(-1);
}

static void click_config_provider(void *context) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 140, up_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 140, down_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_single_handler);
  window_long_click_subscribe(BUTTON_ID_SELECT, 500, select_long_handler, NULL);
  window_single_click_subscribe(BUTTON_ID_BACK, back_handler);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  static uint8_t tick_count = 0;
  
  reset_if_new_day();
  
  tick_count++;
  if (tick_count >= 2) {
    tick_count = 0;
    s_anim_on = !s_anim_on;
    layer_mark_dirty(s_canvas_layer);
  }
}

static void inbox_received_callback(DictionaryIterator *iter, void *context) {
  Tuple *goal_t = dict_find(iter, APP_KEYS[0]);
  Tuple *unit_t = dict_find(iter, APP_KEYS[1]);

  if (goal_t) {
    s_state.goal_ml = goal_t->value->int32;
  }
  if (unit_t) {
    s_state.unit = (uint8_t)unit_t->value->uint8;
  }

  for (int i = 0; i < MAX_AMOUNTS; i++) {
    Tuple *amount_t = dict_find(iter, APP_KEYS[2 + i]);
    if (amount_t) {
      s_state.amounts_ml[i] = amount_t->value->int32;
    }
  }

  save_state();
  layer_mark_dirty(s_canvas_layer);
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "Message dropped: %d", reason);
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, canvas_update);
  layer_add_child(window_layer, s_canvas_layer);
}

static void main_window_unload(Window *window) {
  layer_destroy(s_canvas_layer);
}

static void load_state(void) {
  memset(&s_state, 0, sizeof(s_state));
  s_state.goal_ml = 2800;
  s_state.unit = UNIT_ML;
  s_state.amounts_ml[0] = 250;
  s_state.amounts_ml[1] = 500;
  s_state.amounts_ml[2] = -250;
  s_state.amounts_ml[3] = 1000;

  if (persist_exists(STORAGE_KEY_STATE)) {
    persist_read_data(STORAGE_KEY_STATE, &s_state, sizeof(s_state));
    if (s_state.goal_ml <= 0) {
      s_state.goal_ml = 2800;
    }
  }

  reset_if_new_day();
  save_state();
}

static void init(void) {
  load_state();

  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });
  window_set_click_config_provider(s_main_window, click_config_provider);
  window_stack_push(s_main_window, true);

  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_open(256, 64);

  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
