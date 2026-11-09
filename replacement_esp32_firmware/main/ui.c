#include <stdio.h>
#include "ui.h"
#include "app.h"
#include "sense.h"
#include "power.h"
#include "settings.h"
#include "buttons.h"
#include "display.h"
#include "net.h"
#include "telemetry.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

typedef enum { SCR_DASH = 0, SCR_DETAIL, SCR_SET, SCR_N } scr_t;

#define C_GREY   lv_color_hex(0x8a8a8a)
#define C_WHITE  lv_color_hex(0xffffff)
#define C_YELLOW lv_color_hex(0xffd23f)
#define C_GREEN  lv_color_hex(0x3ddc84)
#define C_BLUE   lv_color_hex(0x4aa3ff)
#define C_RED    lv_color_hex(0xff4040)
#define C_ORANGE lv_color_hex(0xff9a1f)

typedef struct {
    const char *name;
    int32_t *val;
    int32_t step, min, max;
    int div;            // 1000 -> volts from mV, 10 -> amps from dA, 1 -> plain, 0 -> ON/OFF
    const char *unit;
} item_t;

static item_t s_items[] = {
    { "Absorb",  &g_set.absorb_mv,   50, 12000, 15500, 1000, "V" },
    { "Float",   &g_set.float_mv,    50, 12000, 14500, 1000, "V" },
    { "Max chg", &g_set.max_chg_da,  10,    10,   600,   10, "A" },
    { "Load",    &g_set.load_en,      1,     0,     1,    0, ""  },
    { "LVD",     &g_set.lvd_mv,      50, 10000, 12500, 1000, "V" },
    { "LVR",     &g_set.lvr_mv,      50, 11000, 13500, 1000, "V" },
    { "Bklight", &g_set.backlight,   10,    10,   100,    1, "%" },
};
#define N_ITEMS (sizeof(s_items) / sizeof(s_items[0]))

static lv_obj_t *s_scr[SCR_N];
static int s_cur = SCR_DASH;
static lv_obj_t *l_state, *l_tag, *l_pv, *l_pw, *l_bat, *l_load, *l_misc;
static lv_obj_t *l_detail;
static lv_obj_t *l_item[N_ITEMS];
static int s_sel;
static bool s_edit;
static int32_t s_backup;

static lv_obj_t *mk_screen(void)
{
    lv_obj_t *s = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(s, false);
    return s;
}

static lv_obj_t *mk_label(lv_obj_t *p, const lv_font_t *f, lv_color_t c, lv_align_t a, int x, int y, const char *txt)
{
    lv_obj_t *l = lv_label_create(p);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, c, 0);
    lv_obj_align(l, a, x, y);
    lv_label_set_text(l, txt);
    return l;
}

static lv_color_t state_color(chg_state_t s)
{
    switch (s) {
    case CHG_BULK:   return C_YELLOW;
    case CHG_ABSORB: return C_ORANGE;
    case CHG_FLOAT:  return C_GREEN;
    case CHG_FAULT:  return C_RED;
    default:         return C_GREY;
    }
}

static void build_dash(lv_obj_t *s)
{
    l_state = mk_label(s, &lv_font_montserrat_20, C_GREY, LV_ALIGN_TOP_LEFT, 8, 6, "");
    l_tag   = mk_label(s, &lv_font_montserrat_14, C_RED, LV_ALIGN_TOP_RIGHT, -8, 10, "");
    mk_label(s, &lv_font_montserrat_14, C_GREY, LV_ALIGN_TOP_LEFT, 8, 38, "SOLAR");
    l_pv    = mk_label(s, &lv_font_montserrat_20, C_YELLOW, LV_ALIGN_TOP_LEFT, 8, 54, "");
    l_pw    = mk_label(s, &lv_font_montserrat_28, C_WHITE, LV_ALIGN_TOP_MID, 0, 84, "");
    mk_label(s, &lv_font_montserrat_14, C_GREY, LV_ALIGN_TOP_LEFT, 8, 124, "BATTERY");
    l_bat   = mk_label(s, &lv_font_montserrat_20, C_BLUE, LV_ALIGN_TOP_LEFT, 8, 140, "");
    l_load  = mk_label(s, &lv_font_montserrat_14, C_WHITE, LV_ALIGN_TOP_LEFT, 8, 178, "");
    l_misc  = mk_label(s, &lv_font_montserrat_14, C_GREY, LV_ALIGN_TOP_LEFT, 8, 204, "");
}

static void build_detail(lv_obj_t *s)
{
    mk_label(s, &lv_font_montserrat_20, C_WHITE, LV_ALIGN_TOP_LEFT, 8, 6, "Details");
    l_detail = mk_label(s, &lv_font_montserrat_14, C_WHITE, LV_ALIGN_TOP_LEFT, 8, 36, "");
}

static void build_set(lv_obj_t *s)
{
    mk_label(s, &lv_font_montserrat_20, C_WHITE, LV_ALIGN_TOP_LEFT, 8, 6, "Settings");
    for (int i = 0; i < N_ITEMS; i++) {
        lv_obj_t *l = mk_label(s, &lv_font_montserrat_20, C_WHITE, LV_ALIGN_TOP_LEFT, 4, 34 + i * 29, "");
        lv_obj_set_width(l, 232);
        lv_obj_set_style_pad_hor(l, 4, 0);
        lv_obj_set_style_radius(l, 4, 0);
        l_item[i] = l;
    }
}

static void fmt_item(const item_t *it, char *buf, size_t n)
{
    int32_t v = *it->val;
    if (it->div == 0)         snprintf(buf, n, "%-8s %s", it->name, v ? "ON" : "OFF");
    else if (it->div == 1000) snprintf(buf, n, "%-8s %.2f %s", it->name, v / 1000.0, it->unit);
    else if (it->div == 10)   snprintf(buf, n, "%-8s %.1f %s", it->name, v / 10.0, it->unit);
    else                      snprintf(buf, n, "%-8s %ld %s", it->name, (long)v, it->unit);
}

static void refresh(void)
{
    char b[256];
    meas_t m;
    sense_get(&m);
    chg_state_t st = power_state();

    if (s_cur == SCR_DASH) {
        lv_label_set_text(l_state, power_state_name(st));
        lv_obj_set_style_text_color(l_state, state_color(st), 0);
        const char *tag = (st == CHG_FAULT) ? power_fault()
                        : ((g_set.cal_mask & CAL_REQUIRED_MASK) != CAL_REQUIRED_MASK) ? "UNCAL"
                        : !power_stage_enabled() ? "LOCKED" : "";
        lv_label_set_text(l_tag, tag);
        snprintf(b, sizeof(b), "%.1f V", m.pv_v);
        lv_label_set_text(l_pv, b);
        snprintf(b, sizeof(b), "%.0f W", m.out_w > 0 ? m.out_w : 0);
        lv_label_set_text(l_pw, b);
        snprintf(b, sizeof(b), "%.2f V   %.1f A", m.bat_v, m.chg_a);
        lv_label_set_text(l_bat, b);
        snprintf(b, sizeof(b), "LOAD %s   %.1f A", power_load_on() ? "ON" : "OFF", m.load_a);
        lv_label_set_text(l_load, b);
        if (m.temp_ok) snprintf(b, sizeof(b), "%.0f°C   duty %.0f%%%s", m.temp_c, power_duty() * 100, power_fan_on() ? "   FAN" : "");
        else           snprintf(b, sizeof(b), "NTC ERR   duty %.0f%%%s", power_duty() * 100, power_fan_on() ? "   FAN" : "");
        lv_label_set_text(l_misc, b);
    } else if (s_cur == SCR_DETAIL) {
        snprintf(b, sizeof(b),
                 "PV    %6.0f mV  %6.2f V\n"
                 "BAT   %6.0f mV  %6.2f V\n"
                 "ICHG  %6.0f mV  %6.2f A\n"
                 "ILOAD %6.0f mV  %6.2f A\n"
                 "NTC   %6.0f mV  %6.1f C\n"
                 "state %s  duty %.3f\n"
                 "fault %s\n"
                 "pwr %s  cal 0x%02lx\n"
                 "net %s %s  mqtt %s\n"
                 "up %lld s  heap %u",
                 m.pin_mv[SENSE_PV], m.pv_v, m.pin_mv[SENSE_BAT], m.bat_v,
                 m.pin_mv[SENSE_ICHG], m.chg_a, m.pin_mv[SENSE_ILOAD], m.load_a,
                 m.pin_mv[SENSE_NTC], m.temp_c, power_state_name(st), power_duty(),
                 power_fault()[0] ? power_fault() : "-",
                 power_stage_enabled() ? "ENABLED" : "locked", (unsigned long)g_set.cal_mask,
                 net_connected() ? net_ssid() : "down", net_ip(),
                 telemetry_mqtt_ok() ? "ok" : "-",
                 esp_timer_get_time() / 1000000, (unsigned)heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
        lv_label_set_text(l_detail, b);
    } else {
        for (int i = 0; i < N_ITEMS; i++) {
            fmt_item(&s_items[i], b, sizeof(b));
            lv_label_set_text(l_item[i], b);
            bool sel = (i == s_sel);
            lv_obj_set_style_bg_opa(l_item[i], sel ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
            lv_obj_set_style_bg_color(l_item[i], s_edit ? C_ORANGE : lv_color_hex(0x304060), 0);
            lv_obj_set_style_text_color(l_item[i], sel && s_edit ? lv_color_black() : C_WHITE, 0);
        }
    }
}

static void show(int s)
{
    s_cur = s;
    lv_screen_load(s_scr[s]);
    refresh();
}

static void handle(const btn_evt_t *e)
{
    if (s_cur == SCR_SET && s_edit) {
        item_t *it = &s_items[s_sel];
        if (e->id == BTN_UP)   { *it->val += it->step; if (*it->val > it->max) *it->val = it->max; }
        if (e->id == BTN_DOWN) { *it->val -= it->step; if (*it->val < it->min) *it->val = it->min; }
        if (e->id == BTN_ENTER) { s_edit = false; settings_save(); if (it->val == &g_set.load_en) power_load_reset_trip(); }
        if (e->id == BTN_ESC)   { *it->val = s_backup; s_edit = false; }
        if (it->val == &g_set.backlight) display_backlight(g_set.backlight);
        return;
    }
    if (e->id == BTN_ESC) { show(e->lng ? SCR_DASH : (s_cur + 1) % SCR_N); return; }

    if (s_cur == SCR_DASH && e->id == BTN_ENTER && e->lng) {   // long ENTER toggles the load
        g_set.load_en = !g_set.load_en;
        power_load_reset_trip();
        settings_save();
    } else if (s_cur == SCR_SET) {
        if (e->id == BTN_UP)   s_sel = (s_sel + N_ITEMS - 1) % N_ITEMS;
        if (e->id == BTN_DOWN) s_sel = (s_sel + 1) % N_ITEMS;
        if (e->id == BTN_ENTER && !e->lng) { s_edit = true; s_backup = *s_items[s_sel].val; }
    }
}

static void ui_timer(lv_timer_t *t)     // runs in the LVGL task with the lock held
{
    static int n;
    btn_evt_t e;
    bool any = false;
    while (buttons_get(&e)) { handle(&e); any = true; }
    if (any || ++n % 2 == 0) refresh();
}

void ui_init(void)
{
    lvgl_port_lock(0);
    for (int i = 0; i < SCR_N; i++) s_scr[i] = mk_screen();
    build_dash(s_scr[SCR_DASH]);
    build_detail(s_scr[SCR_DETAIL]);
    build_set(s_scr[SCR_SET]);
    show(SCR_DASH);
    lv_timer_create(ui_timer, 100, NULL);
    lvgl_port_unlock();
}
