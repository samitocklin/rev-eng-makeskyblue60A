#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "console.h"
#include "app.h"
#include "sense.h"
#include "power.h"
#include "settings.h"
#include "net.h"
#include "telemetry.h"
#include "esp_console.h"
#include "esp_system.h"

static const char *s_names[SENSE_N] = { "pv", "bat", "ichg", "iload", "ntc" };

static int ch_by_name(const char *n)
{
    for (int i = 0; i < SENSE_ILOAD + 1; i++) if (!strcmp(n, s_names[i])) return i;
    return -1;
}

static int cmd_status(int argc, char **argv)
{
    meas_t m;
    sense_get(&m);
    printf("PV    pin %6.0f mV -> %7.2f V\n", m.pin_mv[SENSE_PV], m.pv_v);
    printf("BAT   pin %6.0f mV -> %7.2f V\n", m.pin_mv[SENSE_BAT], m.bat_v);
    printf("ICHG  pin %6.0f mV -> %7.2f A\n", m.pin_mv[SENSE_ICHG], m.chg_a);
    printf("ILOAD pin %6.0f mV -> %7.2f A\n", m.pin_mv[SENSE_ILOAD], m.load_a);
    printf("NTC   pin %6.0f mV -> %s %.1f C\n", m.pin_mv[SENSE_NTC], m.temp_ok ? "" : "(invalid)", m.temp_c);
    printf("state %s  duty %.3f  fault '%s'  load %s  fan %s  stage %s\n",
           power_state_name(power_state()), power_duty(), power_fault(),
           power_load_on() ? "on" : "off", power_fan_on() ? "on" : "off",
           power_stage_enabled() ? "ENABLED" : "locked");
    for (int i = 0; i < SENSE_NTC; i++)
        printf("cal %-5s gain %9.4f  offset %.4f V  %s\n", s_names[i], g_set.gain[i], g_set.offset_v[i],
               (g_set.cal_mask & (1u << i)) ? "calibrated" : "default");
    return 0;
}

// cal <ch> <true value>: with a multimeter reading, gain = value / (pin_V - offset)
static int cmd_cal(int argc, char **argv)
{
    if (argc != 3) { printf("usage: cal <pv|bat|ichg|iload> <measured value in V or A>\n"); return 1; }
    int c = ch_by_name(argv[1]);
    if (c < 0) { printf("unknown channel\n"); return 1; }
    meas_t m;
    sense_get(&m);
    float pin = m.pin_mv[c] / 1000.0f - g_set.offset_v[c];
    if (pin < 0.05f) { printf("pin signal too small (%.3f V), apply a larger value\n", pin); return 1; }
    g_set.gain[c] = strtof(argv[2], NULL) / pin;
    g_set.cal_mask |= 1u << c;
    settings_save();
    printf("%s gain = %.4f (saved)\n", s_names[c], g_set.gain[c]);
    return 0;
}

// zero <ch>: with zero current flowing, the pin voltage becomes the offset
static int cmd_zero(int argc, char **argv)
{
    if (argc != 2) { printf("usage: zero <ichg|iload|pv|bat>\n"); return 1; }
    int c = ch_by_name(argv[1]);
    if (c < 0) { printf("unknown channel\n"); return 1; }
    meas_t m;
    sense_get(&m);
    g_set.offset_v[c] = m.pin_mv[c] / 1000.0f;
    settings_save();
    printf("%s offset = %.4f V (saved)\n", s_names[c], g_set.offset_v[c]);
    return 0;
}

static int cmd_defaults(int argc, char **argv)
{
    settings_defaults();
    settings_save();
    printf("settings and calibration reset to defaults\n");
    return 0;
}

static int cmd_reboot(int argc, char **argv) { esp_restart(); return 0; }

static int cmd_wifi(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "off")) { net_disable(); printf("wifi disabled\n"); return 0; }
    if (argc < 2) {
        printf("wifi %s ip %s rssi %d\n", net_ssid()[0] ? net_ssid() : "(not configured)",
               net_ip()[0] ? net_ip() : "-", net_rssi());
        return 0;
    }
    net_set_credentials(argv[1], argc > 2 ? argv[2] : "");
    printf("saving and connecting to '%s'\n", argv[1]);
    return 0;
}

static int cmd_mqtt(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "off")) { telemetry_set_broker("", NULL); printf("mqtt disabled\n"); return 0; }
    if (argc < 2) { printf("mqtt %s topic %s (%s)\n", "see NVS", telemetry_topic(),
                           telemetry_mqtt_ok() ? "connected" : "offline"); return 0; }
    telemetry_set_broker(argv[1], argc > 2 ? argv[2] : NULL);
    printf("mqtt broker set\n");
    return 0;
}

static int cmd_net(int argc, char **argv)
{
    printf("wifi: %s  ip %s  rssi %d\n", net_connected() ? "connected" : "down",
           net_ip()[0] ? net_ip() : "-", net_rssi());
    printf("mqtt: %s  topic %s\n", telemetry_mqtt_ok() ? "connected" : "offline", telemetry_topic());
    return 0;
}

void console_init(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t rc = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    rc.prompt = "msb60>";
    esp_console_dev_usb_serial_jtag_config_t hw = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hw, &rc, &repl));
    esp_console_register_help_command();
    const esp_console_cmd_t cmds[] = {
        { .command = "status",   .help = "Show measurements, state and calibration", .func = cmd_status },
        { .command = "cal",      .help = "cal <pv|bat|ichg|iload> <true value>: set gain from a meter reading", .func = cmd_cal },
        { .command = "zero",     .help = "zero <ichg|iload>: store the zero-current offset", .func = cmd_zero },
        { .command = "defaults", .help = "Reset settings and calibration", .func = cmd_defaults },
        { .command = "wifi",     .help = "wifi <ssid> [password] | wifi off", .func = cmd_wifi },
        { .command = "mqtt",     .help = "mqtt <uri e.g. mqtt://host:1883> [topic] | mqtt off", .func = cmd_mqtt },
        { .command = "net",      .help = "Show network status", .func = cmd_net },
        { .command = "reboot",   .help = "Restart", .func = cmd_reboot },
    };
    for (int i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) ESP_ERROR_CHECK(esp_console_cmd_register(&cmds[i]));
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
