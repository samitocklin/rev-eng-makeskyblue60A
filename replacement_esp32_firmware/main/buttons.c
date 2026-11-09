#include "buttons.h"
#include "pins.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#define TICK_MS        10
#define LONG_TICKS     80   // 0.8 s
#define REPEAT_DELAY   50   // 0.5 s
#define REPEAT_EVERY   12   // 120 ms

static const gpio_num_t s_pin[BTN_N] = { PIN_BTN_ESC, PIN_BTN_UP, PIN_BTN_DOWN, PIN_BTN_ENTER };
static QueueHandle_t s_q;

static void send(int id, bool lng)
{
    btn_evt_t e = { .id = id, .lng = lng };
    xQueueSend(s_q, &e, 0);
}

static void btn_task(void *arg)
{
    uint8_t hist[BTN_N] = { 0 };
    bool st[BTN_N] = { 0 }, long_sent[BTN_N] = { 0 };
    int held[BTN_N] = { 0 };
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(TICK_MS));
        for (int i = 0; i < BTN_N; i++) {
            hist[i] = (hist[i] << 1) | !gpio_get_level(s_pin[i]);
            bool p = st[i];
            if ((hist[i] & 0x0F) == 0x0F) p = true;
            else if ((hist[i] & 0x0F) == 0) p = false;
            bool updown = (i == BTN_UP || i == BTN_DOWN);

            if (p && !st[i]) {
                held[i] = 0; long_sent[i] = false;
                if (updown) send(i, false);
            } else if (p) {
                held[i]++;
                if (updown) {
                    if (held[i] >= REPEAT_DELAY && (held[i] - REPEAT_DELAY) % REPEAT_EVERY == 0) send(i, false);
                } else if (held[i] == LONG_TICKS) {
                    send(i, true); long_sent[i] = true;
                }
            } else if (st[i] && !updown && !long_sent[i]) {
                send(i, false);
            }
            st[i] = p;
        }
    }
}

void buttons_init(void)
{
    uint64_t mask = 0;
    for (int i = 0; i < BTN_N; i++) mask |= 1ULL << s_pin[i];
    gpio_config_t c = { .pin_bit_mask = mask, .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE };
    gpio_config(&c);
    s_q = xQueueCreate(16, sizeof(btn_evt_t));
    xTaskCreate(btn_task, "buttons", 2048, NULL, 5, NULL);
}

bool buttons_get(btn_evt_t *e)
{
    return xQueueReceive(s_q, e, 0) == pdTRUE;
}
