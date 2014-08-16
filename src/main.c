#include "pebble.h"
#include "main.h"

// #define DEBUG

static Window *window;

static Layer *current_layer = NULL;
static TextLayer *clock_layer = NULL;
static TextLayer *pm_layer = NULL;
static TextLayer *date_layer = NULL;
static GBitmap *bt_icon = NULL;
static BitmapLayer *bt_layer = NULL;
static GBitmap *batt_icon = NULL;
static BitmapLayer *batt_layer = NULL;

static TextLayer *curr_temp_layer = NULL;

static Layer *forecast_layer = NULL;
static TextLayer *forecast_day_layer = NULL;
static TextLayer *status_layer = NULL;
static TextLayer *city_layer = NULL;
static TextLayer *condition_layer = NULL;
static TextLayer *high_temp_layer = NULL;
static TextLayer *high_label_layer = NULL;
static TextLayer *low_temp_layer = NULL;
static TextLayer *low_label_layer = NULL;
static TextLayer *sun_rise_set_layer = NULL;

static BitmapLayer *icon_layer = NULL;
static GBitmap *icon_bitmap = NULL;

static BitmapLayer *sun_layer = NULL;
static GBitmap *sunrise_bitmap = NULL;
static GBitmap *sunset_bitmap = NULL;

static Layer *cal_layer = NULL;
static InverterLayer *curr_date_layer = NULL;

static InverterLayer *daymode_layer = NULL;

static const int startday = 1;
static char *weekdays[7] = { "Su", "Mo", "Tu", "We", "Th", "Fr", "Sa" };

static char current_time[] = "00:00";
static char current_date[] = "Sun Jan 01";

static char status[50] = "Starting...";
static char city[50] = "";
static char curr_temp[10] = "";
static char sun_rise_set[6] = "";
static char forecast_day[9] = "";
static char high_temp[10] = "";
static char low_temp[10] = "";
static int icon = 0;
static char condition[50] = "";
static int sun_rise_hour = 99;
static int sun_rise_min = 99;
static int sun_set_hour = 99;
static int sun_set_min = 99;
static int prev_daytime = 99;
static bool is_fetching = false;
static bool is_loaded = false;

static const uint32_t const vibe_segments[] = { 100, 200, 100 };
static VibePattern vibe_pattern = {
	.durations = vibe_segments,
	.num_segments = ARRAY_LENGTH(vibe_segments),
};

// App Message Keys for Tuples transferred from Javascript
enum WeatherKey {
	WEATHER_STATUS_KEY = 0,
	WEATHER_CURR_TEMP_KEY = 1,
	WEATHER_SUN_RISE_SET_KEY = 2,
	WEATHER_FORECAST_DAY_KEY = 3,
	WEATHER_HIGH_TEMP_KEY = 4,
	WEATHER_LOW_TEMP_KEY = 5,
	WEATHER_ICON_KEY = 6,
	WEATHER_CONDITION_KEY = 7,
	WEATHER_CITY_KEY = 8,
	WEATHER_SUN_RISE_HOUR_KEY = 9,
	WEATHER_SUN_RISE_MIN_KEY = 10,
	WEATHER_SUN_SET_HOUR_KEY = 11,
	WEATHER_SUN_SET_MIN_KEY = 12
};

// Weather icon resources defined in order to match Javascript icon values
static const uint32_t const WEATHER_ICONS[] = {
	RESOURCE_ID_IMAGE_NA, //0
	RESOURCE_ID_IMAGE_SUNNY, //1
	RESOURCE_ID_IMAGE_PARTLYCLOUDY, //2
	RESOURCE_ID_IMAGE_CLOUDY, //3
	RESOURCE_ID_IMAGE_WINDY, //4
	RESOURCE_ID_IMAGE_LOWVISIBILITY, //5
	RESOURCE_ID_IMAGE_ISOLATEDTHUNDERSTORMS, //6
	RESOURCE_ID_IMAGE_SCATTEREDTHUNDERSTORMS, //7
	RESOURCE_ID_IMAGE_DRIZZLE, //8
	RESOURCE_ID_IMAGE_RAIN, //9
	RESOURCE_ID_IMAGE_HAIL, //10
	RESOURCE_ID_IMAGE_SNOW, //11
	RESOURCE_ID_IMAGE_MIXEDSNOW, //12
	RESOURCE_ID_IMAGE_COLD, //13
	RESOURCE_ID_IMAGE_TORNADO, //14
	RESOURCE_ID_IMAGE_STORM, //15
	RESOURCE_ID_IMAGE_LIGHTSNOW, //16
	RESOURCE_ID_IMAGE_HOT, //17
	RESOURCE_ID_IMAGE_HURRICANE //18
};

static TextLayer* init_text_layer(Layer *parent, GRect location, GColor colour, GColor background, const char *res_id, GTextAlignment alignment, GTextOverflowMode overflow) {
	TextLayer *layer = text_layer_create(location);
	text_layer_set_text_color(layer, colour);
	text_layer_set_background_color(layer, background);
	text_layer_set_font(layer, fonts_get_system_font(res_id));
	text_layer_set_text_alignment(layer, alignment);
	text_layer_set_overflow_mode(layer, overflow);
	layer_add_child(parent, text_layer_get_layer(layer));
	return layer;
}

static void update_sun_layer(struct tm *t) {
	if (sun_rise_hour != 99 && sun_rise_min != 99 && sun_set_hour != 99 && sun_set_min != 99) {

		if (t == NULL) {
			// Get current time
			time_t temp;
			temp = time(NULL);
			t = localtime(&temp);
		}

#ifdef DEBUG
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Sun Rise Hour: %d, Sun Rise Minute: %d", sun_rise_hour, sun_rise_min);
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Sun Set Hour: %d, Sun Set Minute: %d", sun_set_hour, sun_set_min);
#endif

		bool daytime = true;

		if (t->tm_hour < sun_rise_hour || (t->tm_hour == sun_rise_hour && t->tm_min <= sun_rise_min) ||
			t->tm_hour > sun_set_hour || (t->tm_hour == sun_set_hour && t->tm_min >= sun_set_min))
			daytime = false;

		if ((daytime && prev_daytime != 1) || (!daytime && prev_daytime != 0)) {
#ifdef DEBUG
			APP_LOG(APP_LOG_LEVEL_DEBUG, "Updating sun layer");
#endif

			if (t->tm_hour < sun_rise_hour || (t->tm_hour == sun_rise_hour && t->tm_min <= sun_rise_min) ||
				t->tm_hour > sun_set_hour || (t->tm_hour == sun_set_hour && t->tm_min >= sun_set_min)) {
				// Night
				snprintf(sun_rise_set, sizeof(sun_rise_set), "%d:%.2d", sun_rise_hour, sun_rise_min);
				bitmap_layer_set_bitmap(sun_layer, sunrise_bitmap);
				prev_daytime = 0;
			}
			else {
				// Day
				snprintf(sun_rise_set, sizeof(sun_rise_set), "%d:%.2d", sun_set_hour, sun_set_min);
				bitmap_layer_set_bitmap(sun_layer, sunset_bitmap);
				prev_daytime = 1;
			}

			layer_set_hidden(inverter_layer_get_layer(daymode_layer), !daytime);

			text_layer_set_text(sun_rise_set_layer, sun_rise_set);
			layer_set_hidden(bitmap_layer_get_layer(sun_layer), false);
		}
	}
	else if (sun_rise_hour == 99 && sun_rise_min == 99 && sun_set_hour == 99 && sun_set_min == 99) {
		text_layer_set_text(sun_rise_set_layer, "");
		layer_set_hidden(bitmap_layer_get_layer(sun_layer), true);
	}
}

// Process incoming message.
static void process_tuple(Tuple *t) {
	//Get key
	int key = t->key;

	//Get integer value, if present
	int value = t->value->int32;

	//Decide what to do
	switch (key) {
	case WEATHER_ICON_KEY:
		if (icon_bitmap) {
			gbitmap_destroy(icon_bitmap);
		}
		icon = value;
		layer_set_hidden(bitmap_layer_get_layer(icon_layer), (icon == 0));
		icon_bitmap = gbitmap_create_with_resource(WEATHER_ICONS[icon]);
		bitmap_layer_set_bitmap(icon_layer, icon_bitmap);
		break;
	case WEATHER_STATUS_KEY:
		strncpy(status, t->value->cstring, sizeof(status));
		text_layer_set_text(status_layer, status);
		break;
	case WEATHER_CITY_KEY:
		strncpy(city, t->value->cstring, sizeof(city));
		text_layer_set_text(city_layer, city);
		break;
	case WEATHER_CURR_TEMP_KEY:
		strncpy(curr_temp, t->value->cstring, sizeof(curr_temp));
		text_layer_set_text(curr_temp_layer, curr_temp);
#ifdef DEBUG
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Displaying Current Temp: %s", curr_temp);
#endif
		break;
	case WEATHER_FORECAST_DAY_KEY:
		strncpy(forecast_day, t->value->cstring, sizeof(forecast_day));
		text_layer_set_text(forecast_day_layer, forecast_day);
		break;
	case WEATHER_HIGH_TEMP_KEY:
		strncpy(high_temp, t->value->cstring, sizeof(high_temp));
		text_layer_set_text(high_temp_layer, high_temp);
		layer_set_hidden(text_layer_get_layer(high_label_layer), strlen(high_temp) == 0);
		break;
	case WEATHER_LOW_TEMP_KEY:
		strncpy(low_temp, t->value->cstring, sizeof(low_temp));
		text_layer_set_text(low_temp_layer, low_temp);
		layer_set_hidden(text_layer_get_layer(low_label_layer), strlen(low_temp) == 0);
		break;
	case WEATHER_CONDITION_KEY:
		strncpy(condition, t->value->cstring, sizeof(condition));
		text_layer_set_text(condition_layer, condition);
		break;
	default:
		break;
	}
}

// On sucess input message.
static void in_received_handler(DictionaryIterator *iter, void *context) {
	is_fetching = false;
	//Get data
	Tuple *tuple = dict_read_first(iter);
	while (tuple)
	{
		process_tuple(tuple);

		//Get next
		tuple = dict_read_next(iter);
	}
	
	// Get Sunset and Sunrise at once.
	tuple = dict_find(iter, WEATHER_SUN_RISE_HOUR_KEY);
	if(!tuple) return;
	int rise_hour = tuple->value->int32;
	tuple = dict_find(iter, WEATHER_SUN_RISE_MIN_KEY);
	if(!tuple) return;
	int rise_min = tuple->value->int32;
	tuple = dict_find(iter, WEATHER_SUN_SET_HOUR_KEY);
	if(!tuple) return;
	int set_hour = tuple->value->int32;
	tuple = dict_find(iter, WEATHER_SUN_SET_MIN_KEY);
	if(!tuple) return;
	int set_min = tuple->value->int32;
	sun_rise_hour = rise_hour;
	sun_rise_min = rise_min;
	sun_set_hour = set_hour;
	sun_set_min = set_min;
	prev_daytime = 99;
	update_sun_layer(NULL);
}

// On dropping input message.
static void in_dropped_handler(AppMessageResult reason, void *context) {
	text_layer_set_text(status_layer, "BT in drop");
	is_fetching = false;
	psleep(500);
	update_weather(); // Try to resend message.
}

// On out sending success.
static void out_sent_handler(DictionaryIterator *iter, void *context) {
	text_layer_set_text(status_layer, "Fetching...");
	is_fetching = true;
}

// On out sending failure.
static void out_failed_handler(DictionaryIterator *iter, AppMessageResult reason, void *context) {
	text_layer_set_text(status_layer, "BT out err");
	psleep(500);
	update_weather(); // Try to resend message.
}

// Procedure that triggers the weather data to update via Javascript
static void update_weather(void) {
	if (!bluetooth_connection_service_peek() || is_fetching || !is_loaded) return;
	Tuplet value = TupletInteger(WEATHER_CITY_KEY, 42);

	DictionaryIterator *iter;
	app_message_outbox_begin(&iter);

	if (iter == NULL) {
		return;
	}

	dict_write_tuplet(iter, &value);

	app_message_outbox_send();
}

// Handle clock change events
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
	if ((units_changed & MINUTE_UNIT) == MINUTE_UNIT) {
		clock_copy_time_string(current_time, sizeof(current_time));
		text_layer_set_text(clock_layer, current_time);
#ifdef DEBUG
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Current time: %s", current_time);
#endif
		if ((tick_time->tm_min + 1) % 17 == 0) {
			update_sun_layer(tick_time); // Update sun layer every hour on 16, 33, 50 min.
		}
		if (tick_time->tm_min >= 3 && is_fetching) { // Weather update stuck... resend.
			snprintf(status, sizeof(status), "Stuck:%s", current_time);
			text_layer_set_text(status_layer, status );
			is_fetching = false;
			update_weather();
		}
	}
	if ((units_changed & HOUR_UNIT) == HOUR_UNIT) {
#ifdef DEBUG
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Hour changed");
#endif
		if (clock_is_24h_style()) {
			text_layer_set_text(pm_layer, "");
		}
		else {
			if (tick_time->tm_hour >= 12) {
				text_layer_set_text(pm_layer, "PM");
			}
			else {
				text_layer_set_text(pm_layer, "AM");
			}
		}
		update_weather(); // Update the weather every 1 hour
	}
	if ((units_changed & DAY_UNIT) == DAY_UNIT) {
#ifdef DEBUG
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Day changed");
#endif
		strftime(current_date, sizeof(current_date), "%a %b %d", tick_time);
		text_layer_set_text(date_layer, current_date);
		// Trigger redraw of calendar
		layer_mark_dirty(cal_layer);
	}
}

// Handle Bluetooth status updates
static void handle_bt_update(bool connected) {
	if (connected) {
#ifdef DEBUG
		APP_LOG(APP_LOG_LEVEL_DEBUG, "BT Connected");
#endif	
		psleep(1000);
		update_weather();
	} else {
#ifdef DEBUG
		APP_LOG(APP_LOG_LEVEL_DEBUG, "BT DISCONNECTED");
#endif
	}
	layer_set_hidden(bitmap_layer_get_layer(bt_layer), !connected);
	vibes_enqueue_custom_pattern(vibe_pattern);
}

// Handle battery status updates
static void handle_batt_update(BatteryChargeState batt_status) {
	if (batt_icon) {
		gbitmap_destroy(batt_icon);
	}

	if (batt_status.is_charging) {
		batt_icon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATT_CHARGE);
	} else if (batt_status.charge_percent > 75) {
		batt_icon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATT_100);
	} else if (batt_status.charge_percent > 50) {
		batt_icon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATT_75);
	} else if (batt_status.charge_percent > 25) {
		batt_icon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATT_50);
	} else {
		batt_icon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATT_25);
	}

	bitmap_layer_set_bitmap(batt_layer, batt_icon);
}

// Draw dates for a single week in the calendar
static void cal_week_draw_dates(GContext *ctx, int start_date, int curr_mon_len, int prev_mon_len, GColor font_color, int ypos) {
	int curr_date;
	char curr_date_str[3];

	graphics_context_set_text_color(ctx, font_color);

	for (int d = 0; d < 7; d++) {
		// Calculate the current date being drawn
		if ((start_date + d) < 1)
			curr_date = start_date + d + prev_mon_len;
		else if ((start_date + d) > curr_mon_len)
			curr_date = start_date + d - curr_mon_len;
		else
			curr_date = start_date + d;

		// Draw the date text in the correct calendar cell
		snprintf(curr_date_str, 3, "%d", curr_date);
		graphics_draw_text(ctx, curr_date_str, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
			GRect((d * 20) + d, ypos, 19, 14), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
	}
}

// Handle drawing of the 3 week calendar layer
static void cal_layer_draw(Layer *layer, GContext *ctx) {
	// Paint calendar background
	graphics_context_set_fill_color(ctx, GColorWhite);
	graphics_fill_rect(ctx, GRect(0, 0, 144, 46), 0, GCornerNone);

	// Paint inverted rows background
	graphics_context_set_fill_color(ctx, GColorBlack);
	graphics_fill_rect(ctx, GRect(0, 11, 144, 11), 0, GCornerNone);
	graphics_fill_rect(ctx, GRect(0, 35, 144, 11), 0, GCornerNone);

	// Draw day separators
	/*graphics_context_set_stroke_color(ctx, GColorWhite);
	for (int c = 1; c < 7; c++) {
	graphics_draw_line(ctx, GPoint((c*20) + c - 1, 11), GPoint((c*20) + c - 1, 46));
	}*/

	// Get current time
	struct tm *t;
	time_t temp;
	temp = time(NULL);
	t = localtime(&temp);

	graphics_context_set_text_color(ctx, GColorBlack);
	GFont curr_font;

	// Draw week day names
	for (int d = 0; d < 7; d++) {
		if (t->tm_wday == ((d + startday) % 7))
			curr_font = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
		else
			curr_font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
		graphics_draw_text(ctx, weekdays[(d + startday) % 7], curr_font, GRect((d * 20) + d, -4, 19, 14),
			GTextOverflowModeFill, GTextAlignmentCenter, NULL);
	}

	// Calculate leap year and month lengths
	int leap_year = (((1900 + t->tm_year) % 100) == 0 ? 0 : (((1900 + t->tm_year) % 4) == 0) ? 1 : 0);
	int prev_mon = (t->tm_mon) == 0 ? 12 : t->tm_mon;
	int curr_mon = t->tm_mon + 1;
	int prev_mon_len = 31 - ((prev_mon == 2) ? (3 - leap_year) : ((prev_mon - 1) % 7 % 2));
	int curr_mon_len = 31 - ((curr_mon == 2) ? (3 - leap_year) : ((curr_mon - 1) % 7 % 2));

	// Draw previous week dates
	cal_week_draw_dates(ctx, t->tm_mday - t->tm_wday - 7 + startday, curr_mon_len, prev_mon_len, GColorWhite, 7);
	// Draw current week dates
	cal_week_draw_dates(ctx, t->tm_mday - t->tm_wday + startday, curr_mon_len, prev_mon_len, GColorBlack, 19);
	// Draw next week dates
	cal_week_draw_dates(ctx, t->tm_mday - t->tm_wday + 7 + startday, curr_mon_len, prev_mon_len, GColorWhite, 31);

	// Invert current date colors to highlight it
	int curr_day = (t->tm_wday + 7 - startday) % 7;
	layer_set_frame(inverter_layer_get_layer(curr_date_layer), GRect((curr_day * 20) + curr_day, 23, 19, 11));
}

static void window_load(Window *window) {
	Layer *window_layer = window_get_root_layer(window);

	sunrise_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SUNRISE);
	sunset_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SUNSET);

	// Setup 'current' layer (time, date, current temp, battery, bluetooth)
	current_layer = layer_create(GRect(0, 0, 144, 58));

	clock_layer = init_text_layer(current_layer, GRect(-1, -13, 126, 50), GColorWhite, GColorClear, FONT_KEY_ROBOTO_BOLD_SUBSET_49, GTextAlignmentCenter, GTextOverflowModeFill);

	pm_layer = init_text_layer(current_layer, GRect(123, 23, 20, 15), GColorWhite, GColorClear, FONT_KEY_GOTHIC_14, GTextAlignmentCenter, GTextOverflowModeFill);

	date_layer = init_text_layer(current_layer, GRect(55, 30, 89, 26), GColorWhite, GColorClear, FONT_KEY_GOTHIC_24_BOLD, GTextAlignmentRight, GTextOverflowModeFill);

	bt_icon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BT);
	bt_layer = bitmap_layer_create(GRect(129, 1, 9, 16));
	bitmap_layer_set_bitmap(bt_layer, bt_icon);
	layer_add_child(current_layer, bitmap_layer_get_layer(bt_layer));
	handle_bt_update(bluetooth_connection_service_peek());

	batt_layer = bitmap_layer_create(GRect(126, 18, 16, 8));
	layer_add_child(current_layer, bitmap_layer_get_layer(batt_layer));
	handle_batt_update(battery_state_service_peek());

	curr_temp_layer = init_text_layer(current_layer, GRect(0, 30, 45, 26), GColorWhite, GColorClear, FONT_KEY_GOTHIC_24, GTextAlignmentLeft, GTextOverflowModeFill);

	layer_add_child(window_layer, current_layer);

	// Setup forecast layer (High/Low Temp, conditions, sunrise/sunset)
	forecast_layer = layer_create(GRect(0, 57, 144, 64));

	forecast_day_layer = init_text_layer(forecast_layer, GRect(0, -4, 64, 17), GColorBlack, GColorWhite, FONT_KEY_GOTHIC_14_BOLD, GTextAlignmentLeft, GTextOverflowModeFill);

	status_layer = init_text_layer(forecast_layer, GRect(60, 48, 84, 14), GColorWhite, GColorClear, FONT_KEY_GOTHIC_14, GTextAlignmentRight, GTextOverflowModeTrailingEllipsis);
	city_layer = init_text_layer(forecast_layer, GRect(60, -4, 84, 17), GColorBlack, GColorWhite, FONT_KEY_GOTHIC_14, GTextAlignmentRight, GTextOverflowModeTrailingEllipsis);

	high_label_layer = init_text_layer(forecast_layer, GRect(1, 6, 10, 24), GColorWhite, GColorClear, FONT_KEY_GOTHIC_24, GTextAlignmentLeft, GTextOverflowModeFill);
	text_layer_set_text(high_label_layer, "H");
	layer_set_hidden(text_layer_get_layer(high_label_layer), true);

	high_temp_layer = init_text_layer(forecast_layer, GRect(9, 6, 45, 24), GColorWhite, GColorClear, FONT_KEY_GOTHIC_24, GTextAlignmentRight, GTextOverflowModeFill);

	low_label_layer = init_text_layer(forecast_layer, GRect(1, 23, 10, 24), GColorWhite, GColorClear, FONT_KEY_GOTHIC_24, GTextAlignmentLeft, GTextOverflowModeFill);
	text_layer_set_text(low_label_layer, "L");
	layer_set_hidden(text_layer_get_layer(low_label_layer), true);

	low_temp_layer = init_text_layer(forecast_layer, GRect(9, 23, 45, 24), GColorWhite, GColorClear, FONT_KEY_GOTHIC_24, GTextAlignmentRight, GTextOverflowModeFill);

	sun_rise_set_layer = init_text_layer(forecast_layer, GRect(109, 26, 36, 18), GColorWhite, GColorClear, FONT_KEY_GOTHIC_18, GTextAlignmentCenter, GTextOverflowModeFill);

	icon_layer = bitmap_layer_create(GRect(66, 16, 32, 32));
	layer_add_child(forecast_layer, bitmap_layer_get_layer(icon_layer));

	sun_layer = bitmap_layer_create(GRect(117, 17, 20, 14));
	layer_add_child(forecast_layer, bitmap_layer_get_layer(sun_layer));

	condition_layer = init_text_layer(forecast_layer, GRect(0, 43, 144, 24), GColorWhite, GColorClear, FONT_KEY_GOTHIC_18, GTextAlignmentLeft, GTextOverflowModeTrailingEllipsis);

	layer_add_child(window_layer, forecast_layer);

	// Setup 3 week calendar layer
	cal_layer = layer_create(GRect(0, 122, 144, 47));
	layer_add_child(window_layer, cal_layer);
	curr_date_layer = inverter_layer_create(GRect(0, 23, 19, 11));
	layer_add_child(cal_layer, inverter_layer_get_layer(curr_date_layer));

	layer_set_update_proc(cal_layer, cal_layer_draw);

	daymode_layer = inverter_layer_create(GRect(0, 0, 144, 168));
	layer_set_hidden(inverter_layer_get_layer(daymode_layer), true);
	layer_add_child(window_layer, inverter_layer_get_layer(daymode_layer));

	// Get current time
	struct tm *t;
	time_t temp;
	temp = time(NULL);
	t = localtime(&temp);

	// Init time and date
	tick_handler(t, MINUTE_UNIT | HOUR_UNIT | DAY_UNIT);
		
	psleep(1000);
	is_loaded = true;
	update_weather();
}

static void window_unload(Window *window) {
	// Release image resources
	if (icon_bitmap)
		gbitmap_destroy(icon_bitmap);

	if (bt_icon)
		gbitmap_destroy(bt_icon);

	if (batt_icon)
		gbitmap_destroy(batt_icon);

	gbitmap_destroy(sunrise_bitmap);
	gbitmap_destroy(sunset_bitmap);

	// Release UI resources
	text_layer_destroy(clock_layer);
	text_layer_destroy(date_layer);
	text_layer_destroy(pm_layer);
	text_layer_destroy(curr_temp_layer);
	text_layer_destroy(sun_rise_set_layer);
	bitmap_layer_destroy(bt_layer);
	bitmap_layer_destroy(batt_layer);
	layer_destroy(current_layer);

	text_layer_destroy(forecast_day_layer);
	text_layer_destroy(status_layer);
	text_layer_destroy(city_layer);
	text_layer_destroy(high_label_layer);
	text_layer_destroy(high_temp_layer);
	text_layer_destroy(low_label_layer);
	text_layer_destroy(low_temp_layer);
	text_layer_destroy(condition_layer);
	bitmap_layer_destroy(icon_layer);
	bitmap_layer_destroy(sun_layer);
	layer_destroy(forecast_layer);

	inverter_layer_destroy(curr_date_layer);
	layer_destroy(cal_layer);

	inverter_layer_destroy(daymode_layer);
}

static void init(void) {
	window = window_create();
	window_set_background_color(window, GColorBlack);
	window_set_fullscreen(window, true);
	window_set_window_handlers(window, (WindowHandlers) {
		.load = window_load,
		.unload = window_unload
	});

	tick_timer_service_subscribe(MINUTE_UNIT, (TickHandler)tick_handler);
	battery_state_service_subscribe(handle_batt_update);
	bluetooth_connection_service_subscribe(handle_bt_update);
	
	//Register AppMessage events
	app_message_register_inbox_received(in_received_handler);
	app_message_register_inbox_dropped(in_dropped_handler);
	app_message_register_outbox_sent(out_sent_handler);
	app_message_register_outbox_failed(out_failed_handler);
	app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());    //Largest possible input and output buffer sizes
	
	window_stack_push(window, true);
}

static void deinit(void) {
	app_message_deregister_callbacks();
	tick_timer_service_unsubscribe();
	bluetooth_connection_service_unsubscribe();
	battery_state_service_unsubscribe();

	window_destroy(window);
}

int main(void) {
	is_loaded = false;
	init();
	app_event_loop();
	deinit();
}
