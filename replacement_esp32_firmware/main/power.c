#include "power.h"
#include "pins.h"
#include "sense.h"
#include "settings.h"
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "power";

#ifdef CONFIG_MSB_POWER_STAGE_ENABLE
#define STAGE_EN 1
#else
#define STAGE_EN 0
#endif
#ifdef CONFIG_MSB_LOAD_OUTPUT_ENABLE
#define LOAD_OUT_EN 1
#else
#define LOAD_OUT_EN 0
#endif

#define CTRL_HZ          50
#define DUTY_MAX         0.95f
#define DUTY_STEP        0.002f   // CV/CC regulation step per tick
#define PNO_EVERY        10       // P&O every 10 ticks (5 Hz)
#define PNO_STEP         0.01f
#define FAULT_HOLD_US    (10LL * 1000000)
#define PWM_RES          LEDC_TIMER_10_BIT
#define PWM_FULL         1023

static volatile chg_state_t s_state = CHG_OFF;
static volatile float s_duty;
static const char *volatile s_fault = "";
static volatile bool s_load_on, s_load_trip, s_fan_on;

bool power_stage_enabled(void) { return STAGE_EN; }
chg_state_t power_state(void)  { return s_state; }
const char *power_fault(void)  { return s_fault; }
float power_duty(void)         { return s_duty; }
bool power_load_on(void)       { return s_load_on; }
bool power_fan_on(void)        { return s_fan_on; }
void power_load_reset_trip(void) { s_load_trip = false; }

const char *power_state_name(chg_state_t s)
{
    static const char *n[] = { "OFF", "BULK", "ABSORB", "FLOAT", "FAULT" };
    return (s <= CHG_FAULT) ? n[s] : "?";
}

void power_init(void)
{
    // GATE_A's function is not traced yet (U4 TLP350): it stays low permanently.
    const gpio_num_t outs[] = { PIN_GATE_A, PIN_GATE_B, PIN_LOAD_EN, PIN_FAN };
    for (int i = 0; i < sizeof(outs) / sizeof(outs[0]); i++) {
        gpio_set_level(outs[i], 0);
        gpio_config_t c = { .pin_bit_mask = 1ULL << outs[i], .mode = GPIO_MODE_OUTPUT,
                            .pull_down_en = GPIO_PULLDOWN_ENABLE };
        gpio_config(&c);
        gpio_set_level(outs[i], 0);
        gpio_set_drive_capability(outs[i], GPIO_DRIVE_CAP_3);
    }
}

static void pwm_setup(void)
{
#if STAGE_EN
    ledc_timer_config_t t = { .speed_mode = LEDC_LOW_SPEED_MODE, .duty_resolution = PWM_RES,
                              .timer_num = LEDC_TIMER_1, .freq_hz = CONFIG_MSB_PWM_FREQ_HZ,
                              .clk_cfg = LEDC_AUTO_CLK };
    ESP_ERROR_CHECK(ledc_timer_config(&t));
    ledc_channel_config_t ch = { .gpio_num = PIN_GATE_B, .speed_mode = LEDC_LOW_SPEED_MODE,
                                 .channel = LEDC_CHANNEL_1, .timer_sel = LEDC_TIMER_1, .duty = 0 };
    ESP_ERROR_CHECK(ledc_channel_config(&ch));
    ESP_LOGW(TAG, "POWER STAGE ENABLED, PWM %d Hz on GPIO%d", CONFIG_MSB_PWM_FREQ_HZ, PIN_GATE_B);
#else
    ESP_LOGI(TAG, "power stage locked (dry run): duty is computed, never applied");
#endif
}

static void pwm_apply(float duty)
{
#if STAGE_EN
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, (uint32_t)(duty * PWM_FULL));
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
#else
    (void)duty;
#endif
}

static const char *check_faults(const meas_t *m)
{
    if (m->temp_ok && m->temp_c > 85.0f)                       return "OVERTEMP";
    if (m->bat_v * 1000.0f > g_set.absorb_mv + 1500)           return "BAT OVERVOLT";
    if (m->bat_v < 8.0f)                                       return "NO BATTERY";
    if (m->pv_v * 1000.0f > g_set.pv_max_mv)                   return "PV OVERVOLT";
    if (m->chg_a * 10.0f > g_set.max_chg_da + 50)              return "OVERCURRENT";
    return NULL;
}

static void load_control(const meas_t *m)
{
    float mv = m->bat_v * 1000.0f;
    if (m->load_a * 10.0f > g_set.max_load_da) s_load_trip = true;   // latched until toggled
    if (s_load_on && mv < g_set.lvd_mv) s_load_on = false;
    else if (!s_load_on && mv > g_set.lvr_mv) s_load_on = true;
    bool out = g_set.load_en && s_load_on && !s_load_trip;
    if (!g_set.load_en || s_load_trip) s_load_on = false;
    gpio_set_level(PIN_LOAD_EN, LOAD_OUT_EN && out);
}

static void fan_control(const meas_t *m)
{
    if (!m->temp_ok || m->temp_c > 45.0f) s_fan_on = true;
    else if (m->temp_c < 40.0f)           s_fan_on = false;
    gpio_set_level(PIN_FAN, s_fan_on);
}

static void ctrl_task(void *arg)
{
    TickType_t last = xTaskGetTickCount();
    float duty = 0, last_p = 0;
    int dir = 1, tick = 0;
    int64_t absorb_t0 = 0, fault_t = 0;
    chg_state_t st = CHG_OFF;

    for (;;) {
        vTaskDelayUntil(&last, pdMS_TO_TICKS(1000 / CTRL_HZ));
        int64_t now = esp_timer_get_time();
        meas_t m;
        sense_get(&m);
        fan_control(&m);
        load_control(&m);

        const char *f = check_faults(&m);
        if (f) { s_fault = f; fault_t = now; }
        if (f || (st == CHG_FAULT && now - fault_t < FAULT_HOLD_US)) {
            st = CHG_FAULT; duty = 0;
        } else {
            s_fault = "";
            float vb = m.bat_v, vpv = m.pv_v;
            if (vpv < vb + 0.5f) {                 // no sun
                st = CHG_OFF; duty = 0;
            } else {
                if (st == CHG_OFF || st == CHG_FAULT) {
                    st = CHG_BULK;
                    duty = 0.9f * vb / vpv;        // start below the battery voltage: no inrush
                    last_p = 0; dir = 1;
                }
                float vt = (st == CHG_FLOAT ? g_set.float_mv : g_set.absorb_mv) / 1000.0f;
                float imax = g_set.max_chg_da / 10.0f;

                if (vb > vt || m.chg_a > imax) {
                    duty -= DUTY_STEP;             // CV / CC limit has priority
                } else if (st == CHG_BULK) {
                    if (++tick % PNO_EVERY == 0) { // perturb & observe on output power
                        if (m.out_w < last_p) dir = -dir;
                        duty += dir * PNO_STEP;
                        last_p = m.out_w;
                    }
                } else {
                    duty += DUTY_STEP;             // creep up until the limit holds it
                }

                if (st == CHG_BULK && vb >= vt - 0.05f) { st = CHG_ABSORB; absorb_t0 = now; }
                else if (st == CHG_ABSORB &&
                         (now - absorb_t0 > (int64_t)g_set.absorb_min * 60 * 1000000 || m.chg_a < 1.0f)) {
                    st = CHG_FLOAT;
                } else if (st == CHG_FLOAT && vb < g_set.float_mv / 1000.0f - 0.6f) {
                    st = CHG_BULK;
                }
            }
        }
        if (duty < 0) duty = 0;
        if (duty > DUTY_MAX) duty = DUTY_MAX;
        s_state = st;
        s_duty = duty;
        pwm_apply(duty);
    }
}

void power_start(void)
{
    pwm_setup();
    xTaskCreate(ctrl_task, "power", 3072, NULL, 10, NULL);
}
