static TextLayer* init_text_layer(Layer *parent, GRect location, GColor colour, GColor background, const char *res_id, GTextAlignment alignment, GTextOverflowMode overflow);

static void update_sun_layer(struct tm *t);

static void process_tuple(Tuple *t);

static void in_received_handler(DictionaryIterator *iter, void *context);

static void in_dropped_handler(AppMessageResult reason, void *context);

static void out_sent_handler(DictionaryIterator *iter, void *context);

static void out_failed_handler(DictionaryIterator *iter, AppMessageResult reason, void *context);

static void update_weather(void);

static void tick_handler(struct tm *tick_time, TimeUnits units_changed);

static void handle_bt_update(bool connected);

static void handle_batt_update(BatteryChargeState batt_status);

static void cal_week_draw_dates(GContext *ctx, int start_date, int curr_mon_len, int prev_mon_len, GColor font_color, int ypos);

static void cal_layer_draw(Layer *layer, GContext *ctx);

static void window_load(Window *window);

static void window_unload(Window *window);

static void init(void);

static void deinit(void);