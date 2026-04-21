#include <pebble.h>
#include "state/state.h"
#include "ui/ui.h"
#include "data/intake.h"
#include "data/stats.h"
#include "utils/format.h"

#define REPEAT_STREAK_DIVISOR 3
#define REPEAT_STEP_BASE 10
#define REPEAT_STEP_INCREMENT 15

static const int APP_KEYS_AMOUNTS[] = {2, 3, 4, 5, 9, 10};

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
static bool s_celebration_played_today = false;
static time_t s_last_intake_time = 0;
static int s_last_intake_amount = 0;
static uint8_t s_last_point_count = 0;
static uint8_t s_milestones_hit = 0;
static bool s_show_undo_message = false;
static AppTimer *s_undo_message_timer = NULL;
static AppTimer *s_anim_timer = NULL;

static int progress_step(void) {
  int streak_factor = s_repeat_streak / REPEAT_STREAK_DIVISOR;
  if (streak_factor > 10) {
    streak_factor = 10;
  }
  return REPEAT_STEP_BASE + (streak_factor * REPEAT_STEP_INCREMENT);
}

static void anim_timer_callback(void *data) {
  s_anim_on = !s_anim_on;
  layer_mark_dirty(s_canvas_layer);
  
  // Continue animating if celebration is active
  if (s_celebrating) {
    s_anim_timer = app_timer_register(200, anim_timer_callback, NULL);
  } else {
    s_anim_timer = NULL;
  }
}

static void start_animation(void) {
  if (!s_anim_timer) {
    s_anim_timer = app_timer_register(200, anim_timer_callback, NULL);
  }
}

static void move_view(MainView new_view) {
  s_view = new_view;
  s_edit_goal = false;
  s_edit_amount = false;
  s_selecting_day = false;
  s_repeat_streak = 0;
  // Cancel any running celebration
  s_celebrating = false;
  s_celebration_counter = 0;
  if (s_anim_timer) {
    app_timer_cancel(s_anim_timer);
    s_anim_timer = NULL;
  }
  layer_mark_dirty(s_canvas_layer);
}

static void canvas_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, UI_BG);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // 2px screen border
  graphics_context_set_stroke_color(ctx, UI_ACCENT);
  graphics_draw_rect(ctx, bounds);
  graphics_draw_rect(ctx, GRect(2, 2, bounds.size.w - 4, bounds.size.h - 4));

  graphics_context_set_text_color(ctx, UI_TEXT);

  DayData *today = ensure_today_day(&s_state);

  UIState ui_state = {
    .state = &s_state,
    .today = today,
    .view = s_view,
    .edit_goal = s_edit_goal,
    .edit_amount = s_edit_amount,
    .selecting_day = s_selecting_day,
    .selected_amount = s_selected_amount,
    .selected_day_offset = s_selected_day_offset,
    .celebrating = s_celebrating,
    .celebration_counter = s_celebration_counter,
    .show_undo_message = s_show_undo_message,
    .milestones_hit = s_milestones_hit,
    .anim_on = s_anim_on,
    .celebration_played_today = s_celebration_played_today
  };

  if (s_celebrating) {
    draw_celebration(ctx, bounds, &ui_state);
    s_celebration_counter++;
    if (s_celebration_counter > 5) {
      s_celebrating = false;
      s_celebration_counter = 0;
    }
    return;
  }

  switch (s_view) {
    case VIEW_MAIN: draw_main_view(ctx, bounds, &ui_state); break;
    case VIEW_AMOUNT: {
      int goal_met = ui_state.today->total_ml >= s_state.goal_ml;
      if (goal_met && !s_celebrating && !s_celebration_played_today) {
        s_celebrating = true;
        s_celebration_counter = 1;
        s_celebration_played_today = true;
        vibes_double_pulse();
        start_animation();
      }
      draw_amount_view(ctx, bounds, &ui_state); 
      break;
    }
    case VIEW_DETAIL: draw_detail_view(ctx, bounds, &ui_state); break;
    case VIEW_WEEKLY: draw_weekly_view(ctx, bounds, &ui_state); break;
    case VIEW_STATS: draw_stats_view(ctx, bounds, &ui_state); break;
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
    graphics_draw_text(ctx, "Undone!", FONT_TITLE,
      GRect(message_box.origin.x, message_box.origin.y + 8, message_box.size.w, 30),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    graphics_context_set_text_color(ctx, UI_TEXT);
  }
}

static void select_single_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_view == VIEW_AMOUNT && !s_edit_amount) {
    DayData *today = ensure_today_day(&s_state);
    s_last_point_count = today->point_count;
    add_intake(&s_state, s_state.amounts_ml[s_selected_amount], &s_milestones_hit,
               &s_last_intake_time, &s_last_intake_amount);
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
  // Exit edit modes first
  if (s_edit_goal || s_edit_amount || s_selecting_day) {
    s_edit_goal = false;
    s_edit_amount = false;
    s_selecting_day = false;
    state_save(&s_state);
    layer_mark_dirty(s_canvas_layer);
    return;
  }
  
  // Navigate back through view hierarchy
  switch (s_view) {
    case VIEW_AMOUNT:
      move_view(VIEW_MAIN);
      break;
    case VIEW_DETAIL:
      move_view(VIEW_MAIN);
      break;
    case VIEW_WEEKLY:
      move_view(VIEW_DETAIL);
      break;
    case VIEW_STATS:
      move_view(VIEW_WEEKLY);
      break;
    case VIEW_MAIN:
      // Let system handle — exit app
      window_stack_pop(true);
      break;
    default:
      window_stack_pop(true);
      break;
  }
}

static void back_long_handler(ClickRecognizerRef recognizer, void *context) {
  // Undo last intake (within 10 seconds)
  time_t now = time(NULL);
  if (s_last_intake_amount != 0 && (now - s_last_intake_time) <= 10) {
    DayData *today = ensure_today_day(&s_state);
    today->total_ml -= s_last_intake_amount;
    if (today->total_ml < 0) today->total_ml = 0;
    today->point_count = s_last_point_count;
    state_save(&s_state);
    s_last_intake_amount = 0;
    s_last_intake_time = 0;
    s_last_point_count = 0;
    s_show_undo_message = true;
    if (s_undo_message_timer) app_timer_cancel(s_undo_message_timer);
    s_undo_message_timer = app_timer_register(2000, hide_undo_message, NULL);
    vibes_short_pulse();
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
    state_save(&s_state);
  } else if (s_edit_amount) {
    int delta = progress_step() * direction;
    s_state.amounts_ml[s_selected_amount] += delta;
    state_save(&s_state);
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
      if (s_selected_amount == 0) {
        move_view(VIEW_MAIN);
        return;
      }
      s_selected_amount--;
    } else {
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
    move_view(VIEW_STATS);
    return;
  } else if (s_view == VIEW_STATS) {
    if (direction == -1) {
      move_view(VIEW_WEEKLY);
      return;
    }
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
  window_long_click_subscribe(BUTTON_ID_BACK, 700, back_long_handler, NULL);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  reset_if_new_day(&s_state, &s_milestones_hit);
  
  // Reset celebration flag on new day by tracking last reset date
  static int s_last_celebration_reset_key = 0;
  int current_day_key = day_key_from_time(time(NULL));
  if (current_day_key != s_last_celebration_reset_key) {
    s_celebration_played_today = false;
    s_last_celebration_reset_key = current_day_key;
  }
  
  layer_mark_dirty(s_canvas_layer);
}

static void inbox_received_callback(DictionaryIterator *iter, void *context) {
  Tuple *goal_t = dict_find(iter, 0);
  Tuple *unit_t = dict_find(iter, 1);

  if (goal_t) {
    s_state.goal_ml = goal_t->value->int32;
  }
  if (unit_t) {
    s_state.unit = (uint8_t)unit_t->value->uint8;
  }

  for (int i = 0; i < MAX_AMOUNTS; i++) {
    Tuple *amount_t = dict_find(iter, APP_KEYS_AMOUNTS[i]);
    if (amount_t) {
      s_state.amounts_ml[i] = amount_t->value->int32;
    }
  }

  state_save(&s_state);
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

static void init(void) {
  state_load(&s_state);
  reset_if_new_day(&s_state, &s_milestones_hit);
  state_save(&s_state);

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
  if (s_anim_timer) {
    app_timer_cancel(s_anim_timer);
  }
  if (s_undo_message_timer) {
    app_timer_cancel(s_undo_message_timer);
  }
  tick_timer_service_unsubscribe();
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
