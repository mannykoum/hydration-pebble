#include <pebble.h>
#include <stdlib.h>

#define STORAGE_KEY_STATE 1
#define MAX_AMOUNTS 4
#define MAX_DAYS 14
#define MAX_POINTS 32
#define REPEAT_STREAK_DIVISOR 3
#define REPEAT_STEP_BASE 10
#define REPEAT_STEP_INCREMENT 15

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

static const int APP_KEYS[] = {0, 1, 2, 3, 4, 5};

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
  int whole = ml / mul;
  int frac = (abs(ml) % mul) * 10 / mul;
  const char *suffix = s_state.unit == UNIT_ML ? "ml" : (s_state.unit == UNIT_CUPS ? "c" : "pt");
  if (s_state.unit == UNIT_ML) {
    snprintf(buffer, size, "%+d%s", ml, suffix);
  } else {
    if (frac == 0) {
      snprintf(buffer, size, "%+d%s", whole, suffix);
    } else {
      snprintf(buffer, size, "%+d.%d%s", whole, frac, suffix);
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

  idx = oldest_day_index();
  memset(&s_state.days[idx], 0, sizeof(DayData));
  s_state.days[idx].date_key = key;
  return &s_state.days[idx];
}

static void reset_if_new_day(void) {
  (void)ensure_today_day();
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

static void add_intake(int delta_ml) {
  DayData *today = ensure_today_day();
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
}

static int progress_step(void) {
  int streak_factor = s_repeat_streak / REPEAT_STREAK_DIVISOR;
  if (streak_factor > 10) {
    streak_factor = 10;
  }
  return REPEAT_STEP_BASE + streak_factor * REPEAT_STEP_INCREMENT;
}

static void move_view(int direction) {
  int next = ((int)s_view + direction + VIEW_COUNT) % VIEW_COUNT;
  s_view = (MainView)next;
  s_edit_goal = false;
  s_edit_amount = false;
  s_selecting_day = false;
  s_repeat_streak = 0;
  layer_mark_dirty(s_canvas_layer);
}

static void draw_progress_bar(GContext *ctx, GRect frame, int numerator, int denominator, const char *label) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_rect(ctx, frame);

  int fill_width = 0;
  if (denominator > 0 && numerator > 0) {
    int ratio = (numerator * frame.size.w) / denominator;
    fill_width = ratio > frame.size.w ? frame.size.w : ratio;
  }

  if (fill_width > 0) {
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, GRect(frame.origin.x + 1, frame.origin.y + 1, fill_width - 1, frame.size.h - 1), 0, GCornerNone);
  }

  graphics_draw_text(ctx, label, fonts_get_system_font(FONT_KEY_GOTHIC_14),
    GRect(frame.origin.x, frame.origin.y - 16, frame.size.w, 14),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void draw_main_view(GContext *ctx, GRect bounds) {
  DayData *today = ensure_today_day();
  int total = today->total_ml;
  int goal = s_state.goal_ml > 0 ? s_state.goal_ml : 2800;

  char top_label[32];
  snprintf(top_label, sizeof(top_label), "%d/%d ml", total, goal);
  draw_progress_bar(ctx, GRect(8, 24, bounds.size.w - 16, 18), total, goal > 0 ? goal : 1, top_label);

  time_t now = time(NULL);
  struct tm *tm_now = localtime(&now);
  int minutes = tm_now->tm_hour * 60 + tm_now->tm_min;
  int day_progress = (minutes * 100) / (24 * 60);
  char day_label[32];
  snprintf(day_label, sizeof(day_label), "Day: %d%%", day_progress);
  draw_progress_bar(ctx, GRect(8, 62, bounds.size.w - 16, 18), minutes, 24 * 60, day_label);

  graphics_draw_text(ctx,
    s_edit_goal ? "Edit goal: up/down" : "Hold select to edit goal",
    fonts_get_system_font(FONT_KEY_GOTHIC_14),
    GRect(8, 92, bounds.size.w - 16, 28),
    GTextOverflowModeWordWrap,
    GTextAlignmentCenter,
    NULL);
}

static void draw_amount_view(GContext *ctx, GRect bounds) {
  DayData *today = ensure_today_day();
  int goal_met = today->total_ml >= s_state.goal_ml;

  graphics_draw_text(ctx, "( )", fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), GRect(4, 12, 28, 26),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  graphics_draw_text(ctx, "| |", fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), GRect(9, 32, 20, 20),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  graphics_draw_line(ctx, GPoint(8, 52), GPoint(26, 52));

  for (int i = 0; i < MAX_AMOUNTS; i++) {
    char line[20];
    format_amount(s_state.amounts_ml[i], line, sizeof(line));
    GRect row = GRect(34, 12 + i * 24, bounds.size.w - 40, 22);
    if (i == s_selected_amount) {
      graphics_context_set_fill_color(ctx, GColorBlack);
      graphics_fill_rect(ctx, row, 0, GCornerNone);
      graphics_context_set_text_color(ctx, GColorWhite);
    } else {
      graphics_context_set_text_color(ctx, GColorBlack);
    }

    graphics_draw_text(ctx, line, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), row,
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
  graphics_context_set_text_color(ctx, GColorBlack);

  if (goal_met) {
    graphics_draw_text(ctx,
      s_anim_on ? "Goal met!" : "Great job!",
      fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
      GRect(0, bounds.size.h - 20, bounds.size.w, 18),
      GTextOverflowModeTrailingEllipsis,
      GTextAlignmentCenter,
      NULL);
  } else {
    graphics_draw_text(ctx,
      s_edit_amount ? "Edit amount" : "Select=add/remove Hold=edit",
      fonts_get_system_font(FONT_KEY_GOTHIC_14),
      GRect(0, bounds.size.h - 20, bounds.size.w, 18),
      GTextOverflowModeTrailingEllipsis,
      GTextAlignmentCenter,
      NULL);
  }
}

static void draw_detail_view(GContext *ctx, GRect bounds) {
  DayData *day = day_by_offset(s_selected_day_offset);
  int goal = s_state.goal_ml;
  int max_value = goal;
  if (day && day->total_ml > max_value) {
    max_value = day->total_ml;
  }
  if (max_value < 1) {
    max_value = 1;
  }

  GRect plot = GRect(8, 20, bounds.size.w - 16, bounds.size.h - 40);
  graphics_draw_rect(ctx, plot);

  for (int i = 1; i <= 3; i++) {
    int y = plot.origin.y + (plot.size.h * i) / 4;
    graphics_draw_line(ctx, GPoint(plot.origin.x, y), GPoint(plot.origin.x + plot.size.w, y));

    char label[12];
    int mark = max_value - (max_value * i / 4);
    snprintf(label, sizeof(label), "%d", mark);
    graphics_draw_text(ctx, label, fonts_get_system_font(FONT_KEY_GOTHIC_09),
      GRect(plot.origin.x + 2, y - 7, 24, 8), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }

  if (day && day->point_count > 0) {
    GPoint last = GPoint(plot.origin.x, plot.origin.y + plot.size.h);
    for (uint8_t i = 0; i < day->point_count; i++) {
      int x = plot.origin.x + (day->minutes[i] * plot.size.w) / (24 * 60);
      int y = plot.origin.y + plot.size.h - (day->cumulative_ml[i] * plot.size.h) / max_value;
      if (y < plot.origin.y) {
        y = plot.origin.y;
      }
      GPoint point = GPoint(x, y);
      graphics_draw_line(ctx, last, point);
      last = point;
    }
  }

  char heading[32];
  if (s_selected_day_offset == 0) {
    snprintf(heading, sizeof(heading), "Today");
  } else {
    snprintf(heading, sizeof(heading), "%d day%s ago", s_selected_day_offset,
             s_selected_day_offset == 1 ? "" : "s");
  }
  graphics_draw_text(ctx, heading, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
    GRect(8, 0, bounds.size.w - 16, 16), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  graphics_draw_text(ctx,
    s_selecting_day ? "Up/down choose day" : "Select to choose day",
    fonts_get_system_font(FONT_KEY_GOTHIC_14),
    GRect(0, bounds.size.h - 16, bounds.size.w, 16),
    GTextOverflowModeTrailingEllipsis,
    GTextAlignmentCenter,
    NULL);
}

static void draw_weekly_view(GContext *ctx, GRect bounds) {
  int goal = s_state.goal_ml > 0 ? s_state.goal_ml : 2800;
  int chart_w = bounds.size.w - 12;
  int bar_w = chart_w / 7;

  for (int i = 0; i < 7; i++) {
    int offset = 6 - i;
    int total = day_total(offset);
    int fill_h = (total * 50) / goal;
    if (fill_h > 50) {
      fill_h = 50;
    }
    GRect bar = GRect(6 + i * bar_w, 26, bar_w - 2, 50);
    graphics_draw_rect(ctx, bar);
    if (fill_h > 0) {
      graphics_context_set_fill_color(ctx, GColorBlack);
      graphics_fill_rect(ctx, GRect(bar.origin.x + 1, bar.origin.y + bar.size.h - fill_h, bar.size.w - 2, fill_h), 0, GCornerNone);
    }
  }

  graphics_draw_text(ctx, "Last 7 days", fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(0, 2, bounds.size.w, 20), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
  graphics_draw_text(ctx, "Empty=outline Full=black", fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(0, 80, bounds.size.w, 20), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
}

static void canvas_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  graphics_context_set_text_color(ctx, GColorBlack);

  switch (s_view) {
    case VIEW_MAIN: draw_main_view(ctx, bounds); break;
    case VIEW_AMOUNT: draw_amount_view(ctx, bounds); break;
    case VIEW_DETAIL: draw_detail_view(ctx, bounds); break;
    case VIEW_WEEKLY: draw_weekly_view(ctx, bounds); break;
    default: break;
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

static void back_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_edit_goal || s_edit_amount || s_selecting_day) {
    s_edit_goal = false;
    s_edit_amount = false;
    s_selecting_day = false;
    save_state();
    layer_mark_dirty(s_canvas_layer);
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
    s_selected_amount = (s_selected_amount - direction + MAX_AMOUNTS) % MAX_AMOUNTS;
  } else {
    move_view(1);
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
  s_anim_on = !s_anim_on;
  reset_if_new_day();
  layer_mark_dirty(s_canvas_layer);
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

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
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
