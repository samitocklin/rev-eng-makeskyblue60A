// Debounced buttons. UP/DOWN: event on press + auto-repeat. ESC/ENTER: short on release, long at 0.8 s.
#pragma once
#include <stdbool.h>

typedef enum { BTN_ESC = 0, BTN_UP, BTN_DOWN, BTN_ENTER, BTN_N } btn_id_t;
typedef struct { btn_id_t id; bool lng; } btn_evt_t;

void buttons_init(void);
bool buttons_get(btn_evt_t *e);   // non-blocking
