#include <math.h>
#include "sense.h"
#include "settings.h"
#include "pins.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

// NTC assumption (verify on the board): 10k B3950 to GND, 10k pull-up to 3V3
#define NTC_R0     10000.0f
#define NTC_BETA   3950.0f
#define NTC_RPULL  10000.0f
#define VREF       3.3f

static const adc_channel_t s_ch[SENSE_N] = { ADC_CH_PV_V, ADC_CH_BAT_V, ADC_CH_I_CHG, ADC_CH_I_LOAD, ADC_CH_NTC };
static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t s_cali;
static meas_t s_m;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

void sense_get(meas_t *out)
{
    taskENTER_CRITICAL(&s_mux);
    *out = s_m;
    taskEXIT_CRITICAL(&s_mux);
}

static float scaled(sense_ch_t c, float mv)
{
    return g_set.gain[c] * (mv / 1000.0f - g_set.offset_v[c]);
}

static void sense_task(void *arg)
{
    float filt[SENSE_N] = { 0 };
    bool first = true;
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        vTaskDelayUntil(&last, pdMS_TO_TICKS(10));
        for (int i = 0; i < SENSE_N; i++) {
            int sum = 0, raw = 0, mv = 0;
            for (int k = 0; k < 8; k++) { adc_oneshot_read(s_adc, s_ch[i], &raw); sum += raw; }
            raw = sum / 8;
            if (!s_cali || adc_cali_raw_to_voltage(s_cali, raw, &mv) != ESP_OK) mv = raw * 3300 / 4095;
            filt[i] = first ? mv : filt[i] + 0.2f * ((float)mv - filt[i]);
        }
        first = false;

        meas_t m = { 0 };
        for (int i = 0; i < SENSE_N; i++) m.pin_mv[i] = filt[i];
        m.pv_v   = scaled(SENSE_PV, filt[SENSE_PV]);
        m.bat_v  = scaled(SENSE_BAT, filt[SENSE_BAT]);
        m.chg_a  = scaled(SENSE_ICHG, filt[SENSE_ICHG]);
        m.load_a = scaled(SENSE_ILOAD, filt[SENSE_ILOAD]);
        m.out_w  = m.bat_v * m.chg_a;

        float v = filt[SENSE_NTC] / 1000.0f;
        if (v > 0.05f && v < VREF - 0.1f) {
            float r = NTC_RPULL * v / (VREF - v);
            m.temp_c  = 1.0f / (1.0f / 298.15f + logf(r / NTC_R0) / NTC_BETA) - 273.15f;
            m.temp_ok = true;
        }

        taskENTER_CRITICAL(&s_mux);
        s_m = m;
        taskEXIT_CRITICAL(&s_mux);
    }
}

void sense_init(void)
{
    adc_oneshot_unit_init_cfg_t u = { .unit_id = ADC_UNIT_1 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&u, &s_adc));
    adc_oneshot_chan_cfg_t cc = { .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT };
    for (int i = 0; i < SENSE_N; i++) ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc, s_ch[i], &cc));
    adc_cali_curve_fitting_config_t cf = { .unit_id = ADC_UNIT_1, .atten = ADC_ATTEN_DB_12,
                                           .bitwidth = ADC_BITWIDTH_DEFAULT };
    if (adc_cali_create_scheme_curve_fitting(&cf, &s_cali) != ESP_OK) {
        s_cali = NULL;
        ESP_LOGW("sense", "no ADC eFuse calibration, using linear raw->mV");
    }
    xTaskCreate(sense_task, "sense", 3072, NULL, 6, NULL);
}
