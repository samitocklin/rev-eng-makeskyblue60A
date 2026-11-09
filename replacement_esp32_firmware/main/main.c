// msb60-fw: MakeSkyBlue 60A MPPT replacement firmware (ESP32-C5-Zero + GMT130 ST7789)
#include "esp_log.h"
#include "pins.h"
#include "power.h"
#include "settings.h"
#include "sense.h"
#include "buttons.h"
#include "display.h"
#include "ui.h"
#include "console.h"
#include "net.h"
#include "telemetry.h"

void app_main(void)
{
    power_init();               // FIRST: every power output driven low
    settings_load();
    sense_init();
    buttons_init();
    display_init(g_set.backlight);
    ui_init();
    power_start();
    net_init();
    telemetry_init();
    console_init();
    ESP_LOGI("msb60", "up. power stage %s", power_stage_enabled() ? "ENABLED" : "locked");
}
