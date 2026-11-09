// Shared types for msb60-fw
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum { SENSE_PV = 0, SENSE_BAT, SENSE_ICHG, SENSE_ILOAD, SENSE_NTC, SENSE_N } sense_ch_t;

typedef struct {
    float pin_mv[SENSE_N];  // filtered voltage at the ESP pin (mV), for calibration
    float pv_v, bat_v;      // volts
    float chg_a, load_a;    // amps
    float out_w;            // charge power = bat_v * chg_a (PV current isn't measured)
    float temp_c;
    bool  temp_ok;          // false = NTC open/shorted
} meas_t;

typedef enum { CHG_OFF = 0, CHG_BULK, CHG_ABSORB, CHG_FLOAT, CHG_FAULT } chg_state_t;
