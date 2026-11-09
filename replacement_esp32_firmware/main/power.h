// Power stage: safe state, charge state machine, MPPT, load and fan control.
// PWM and load output only reach the pins if enabled in menuconfig (MSB60 firmware menu).
#pragma once
#include "app.h"

void power_init(void);    // call FIRST in app_main: forces every power output low
void power_start(void);   // starts the 50 Hz control task

chg_state_t power_state(void);
const char *power_state_name(chg_state_t s);
const char *power_fault(void);   // "" if no fault
float power_duty(void);          // computed duty 0..1 (applied only if the stage is enabled)
bool power_stage_enabled(void);
bool power_load_on(void);
bool power_fan_on(void);
void power_load_reset_trip(void);
