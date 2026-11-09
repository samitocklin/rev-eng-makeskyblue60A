// ADC sampling: 100 Hz, 8x oversampling, IIR filter
#pragma once
#include "app.h"

void sense_init(void);
void sense_get(meas_t *out);   // thread-safe snapshot
