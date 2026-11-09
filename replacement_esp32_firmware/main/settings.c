#include <string.h>
#include "settings.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "settings";
settings_t g_set;

void settings_defaults(void)
{
    memset(&g_set, 0, sizeof(g_set));
    g_set.version     = SETTINGS_VERSION;
    g_set.absorb_mv   = 14400;
    g_set.float_mv    = 13600;
    g_set.pv_max_mv   = 100000;
    g_set.max_chg_da  = 300;     // 30 A until the hardware is proven
    g_set.absorb_min  = 120;
    g_set.load_en     = 0;
    g_set.lvd_mv      = 11500;
    g_set.lvr_mv      = 12600;
    g_set.max_load_da = 200;
    g_set.backlight   = 80;
    // Placeholder gains, NOT measured. Calibrate with the console `cal` command.
    g_set.gain[SENSE_PV]    = 40.0f;  // V per pin-V
    g_set.gain[SENSE_BAT]   = 20.0f;
    g_set.gain[SENSE_ICHG]  = 20.0f;  // A per pin-V
    g_set.gain[SENSE_ILOAD] = 20.0f;
    g_set.gain[SENSE_NTC]   = 1.0f;   // unused, NTC uses the Beta equation
}

void settings_load(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    settings_defaults();
    nvs_handle_t h;
    if (nvs_open("msb60", NVS_READONLY, &h) != ESP_OK) return;
    settings_t tmp;
    size_t len = sizeof(tmp);
    if (nvs_get_blob(h, "set", &tmp, &len) == ESP_OK && len == sizeof(tmp) && tmp.version == SETTINGS_VERSION) {
        g_set = tmp;
        ESP_LOGI(TAG, "loaded from NVS (cal mask 0x%02x)", (unsigned)g_set.cal_mask);
    } else {
        ESP_LOGW(TAG, "no valid settings in NVS, using defaults");
    }
    nvs_close(h);
}

void settings_save(void)
{
    nvs_handle_t h;
    if (nvs_open("msb60", NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_blob(h, "set", &g_set, sizeof(g_set));
    nvs_commit(h);
    nvs_close(h);
}
