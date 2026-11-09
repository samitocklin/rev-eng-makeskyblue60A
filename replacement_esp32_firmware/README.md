# msb60-fw — replacement firmware for the MakeSkyBlue 60A MPPT

The original NUC120/NUC029 MCU is dead. This firmware runs on a **Waveshare ESP32-C5-Zero** with a
**GMT130-V1.0** (1.3" IPS, 240×240, ST7789, 7-pin SPI with no CS). It is built with ESP-IDF v5.5.

```sh
export IDF_PYTHON_ENV_PATH=~/.espressif/python_env/idf5.5_py3.13_env PATH=/opt/homebrew/bin:$PATH
. ~/esp/esp-idf/export.sh
idf.py build flash monitor          # console is on the C5-Zero USB-C port
```

## Status
- [x] Stage 0, bring-up: power stage held off, TFT test pattern, ADC/button logging.
- [x] Stage 1 (code): modules, LVGL UI, NVS settings, calibration console, charge state machine + P&O MPPT running in **dry-run** (duty computed, never applied). **Verified on hardware: display + console work** (gotcha: GMT130 RES must be tied to 3V3, not GND).
- [x] Stage 4 (code): Wi-Fi station + MQTT JSON telemetry + HTTP `/status` endpoint. Console: `wifi <ssid> [pass]`, `mqtt <uri> [topic]`, `net`.
- [ ] Trace the old MCU footprint so every signal below is confirmed; calibrate the ADCs.
- [ ] Bench test: enable `MSB_POWER_STAGE_ENABLE` with a current-limited PSU as PV and a fused battery.

## Code layout (`main/`)
| File | Job |
|---|---|
| `pins.h` | the pin map below |
| `power.c` | safe state at boot, 50 Hz control task: faults, OFF/BULK/ABSORB/FLOAT, P&O MPPT, CV/CC limits, load LVD/LVR + overcurrent trip, fan hysteresis (on >45 °C, off <40 °C, on if NTC fails) |
| `sense.c` | ADC1 at 100 Hz, 8× oversample, IIR filter; value = gain × (pin V − offset) |
| `settings.c` | NVS blob: charge voltages, current limits, load, backlight, calibration |
| `buttons.c` | debounce; UP/DOWN auto-repeat; ESC/ENTER short + long (0.8 s) |
| `display.c` / `ui.c` | ST7789 via esp_lcd + LVGL 9 (esp_lvgl_port) |
| `console.c` | USB REPL: `status`, `cal`, `zero`, `defaults`, `wifi`, `mqtt`, `net`, `reboot` |
| `net.c` | Wi-Fi station, credentials in NVS, off until `wifi <ssid> <pass>` is set |
| `telemetry.c` | MQTT JSON publish every 5 s + HTTP `GET /status` JSON (Home Assistant can poll it) |

**Partition table:** custom `partitions.csv` (3 MB app on the 4 MB flash) — Wi-Fi + MQTT + httpd pushed the app past the default 1.5 MB.

**Safety locks** (`idf.py menuconfig` → *MSB60 firmware*): `MSB_POWER_STAGE_ENABLE` (PWM on GATE_B, default off) and `MSB_LOAD_OUTPUT_ENABLE` (default off). GATE_A is always held low until its function is traced.

**UI:** ESC cycles Dashboard → Details (raw pin mV, for calibration) → Settings; long ESC = Dashboard. Dashboard: long ENTER toggles the load. Settings: UP/DOWN select, ENTER edit/save, ESC cancel. Tags top-right: `UNCAL`, `LOCKED`, or the fault name.

**Calibration** (`idf.py monitor`, prompt `msb60>`): with no current flowing run `zero ichg` and `zero iload`; then apply known voltages/currents and run e.g. `cal bat 13.20`, `cal pv 36.5`, `cal ichg 5.0` using your meter's reading. Saved to NVS immediately. Default gains are placeholders.

**Assumptions to verify:** NTC is 10k B3950 to GND with a 10k pull-up to 3V3 (`sense.c`); PV current isn't measured, so MPPT maximises output power (bat V × charge A); default PWM 40 kHz (the original frequency is unknown, scope a working unit if you can).

## Pin map

| Zero pin | Function | Notes |
|---|---|---|
| GP1 | ADC PV voltage | ADC1_CH0 |
| GP2 | ADC battery voltage | ADC1_CH1 |
| GP3 | ADC charge current | ADC1_CH2 |
| GP4 | ADC load current | ADC1_CH3 |
| GP5 | ADC heatsink NTC | ADC1_CH4 |
| GP0 | GATE_A → R32 → U4 TLP350 | no pull at reset. **10k pull-down** |
| GP6 | GATE_B → R31 → U5 TLP350 | no pull at reset. **10k pull-down** |
| GP23 (pad) | LOAD_EN | no pull at reset. **10k pull-down** |
| GP11 | FAN → R34 → QF1 | UART0 TX; the boot log only blips the fan |
| GP7 | TFT SDA | strap pin; add 10k to GND |
| GP8 | TFT SCK | |
| GP9 | TFT DC | |
| GP10 | TFT BLK | LEDC PWM dimming |
| — | TFT RES | 10k to 3V3 + 100 nF to GND; firmware also sends SWRESET |
| GP28 (pad) | BTN PRG/ESC | boot strap: hold it during reset to enter download mode |
| GP25 (pad) | BTN UP | |
| GP24 (pad) | BTN DOWN | |
| GP12 | BTN ENTER | UART0 RX, weak pull-up from reset |
| GP13/14 | USB | flashing and console |
| 5V / GND | from the power board's +5V rail | see open item 4 |

TFT header: GND, VCC (3V3), SCK, SDA, RES, DC, BLK.

### Reasoning
- **Analog stays on GP1–GP5.** ADC1 only exists on GP1–GP6, so the display never goes there.
- **Gate and load outputs use pins that are high-Z with no pull at reset** (GP0, GP6, GP23), per ESP32-C5 datasheet Table 2-1. None of them toggles during boot. Each also gets an external pull-down, because pins float until `app_main()` runs.
- **Pins that misbehave at boot get harmless jobs.** GP11 (TX boot log) drives the fan. GP12 (weak pull-up), GP25 and GP28 (pull-up strap) take active-low buttons.
- **The TFT uses 4 adjacent edge pins** (GP7–GP10): one short ribbon. RES uses an RC, which saves a pin.
- **Budget: every usable GPIO is assigned; none is spare.** To free pins later (for UART/RS485 or TFT RES), move the 4 buttons onto one resistor ladder. That needs an ADC pin, so GATE_B would move to GP12.

## Hardware notes / open items (verify before switching power)
1. **The schematic doesn't capture the MCU side.** The KiCad file covers the power stage, but none of the old MCU pins are netted. Before Stage 3, trace each signal in the table above from the dead MCU footprint (LQFP48) to the power board.
2. **TLP350 LED current.** IFLH (threshold) is up to **5 mA**, and the recommended IF(ON) is 7.5–10 mA. From 3.3 V through the existing **470 Ω** you get about (3.3−1.6)/470 ≈ **3.6 mA**, which will not switch reliably. Change R31/R32 to **150 Ω** (about 10 mA). First check how the LED cathode (pin 3) is wired; if it goes to +5V rather than GND, the drive scheme changes.
3. **ADC scaling.** If the old MCU ran at 5 V, the dividers and amps may output more than 3.3 V. Measure each analog node at maximum PV/battery voltage, and re-scale or clamp it to ≤ 3.1 V (12 dB attenuation range) before connecting it.
4. **+5V rail capacity.** The C5 Wi-Fi peak is about 350 mA, and the backlight adds about 20–30 mA. Confirm the aux supply (UC3845 flyback) and R12 (10 Ω in the +5V path?) can supply that without sagging.
5. **Safety.** Until the control loop has hardware-verified current limits, test with a current-limited bench PSU as the "PV" input and a fused battery.
