// Persistent settings (NVS blob)
#pragma once
#include "app.h"

#define SETTINGS_VERSION 1
#define CAL_REQUIRED_MASK ((1u << SENSE_PV) | (1u << SENSE_BAT) | (1u << SENSE_ICHG) | (1u << SENSE_ILOAD))

typedef struct {
    uint32_t version;
    // charging (12 V lead-acid defaults)
    int32_t absorb_mv, float_mv, pv_max_mv;
    int32_t max_chg_da;        // max charge current, 0.1 A units
    int32_t absorb_min;        // absorb phase time limit, minutes
    // load output
    int32_t load_en, lvd_mv, lvr_mv, max_load_da;
    // UI
    int32_t backlight;         // %
    // calibration: value = gain * (pin_V - offset_V)
    float gain[SENSE_N];
    float offset_v[SENSE_N];
    uint32_t cal_mask;         // bit per channel that has been calibrated
} settings_t;

extern settings_t g_set;

void settings_load(void);
void settings_save(void);
void settings_defaults(void);
