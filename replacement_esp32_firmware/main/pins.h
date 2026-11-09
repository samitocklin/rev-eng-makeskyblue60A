// Pin map: Waveshare ESP32-C5-Zero -> MakeSkyBlue 60A power board + GMT130-V1.0 (ST7789 240x240)
//
// Usable pins on the Zero: GP0-GP12 on the edges (GP13/14 are USB), and GP23/24/25/28 as pads on the bottom.
// The board itself uses GP26 (antenna switch), GP27 (onboard) and GP29 (WS2812).
// Reset states are from the ESP32-C5 datasheet v1.5, Table 2-1 and Table 3-1.
#pragma once
#include "driver/gpio.h"
#include "hal/adc_types.h"

// ---- Analog: ADC1 exists only on GP1..GP6. GP1..GP5 are reserved for measurements ----
#define PIN_ADC_PV_V      GPIO_NUM_1   // ADC1_CH0  PV voltage divider
#define PIN_ADC_BAT_V     GPIO_NUM_2   // ADC1_CH1  battery voltage divider (MTMS strap: JTAG/SDIO only, harmless)
#define PIN_ADC_I_CHG     GPIO_NUM_3   // ADC1_CH2  charge current (shunt amp) (MTDI strap: SDIO edge only)
#define PIN_ADC_I_LOAD    GPIO_NUM_4   // ADC1_CH3  load current (weak pull-up at reset, cleared by the ADC init)
#define PIN_ADC_NTC       GPIO_NUM_5   // ADC1_CH4  heatsink NTC
#define ADC_CH_PV_V       ADC_CHANNEL_0
#define ADC_CH_BAT_V      ADC_CHANNEL_1
#define ADC_CH_I_CHG      ADC_CHANNEL_2
#define ADC_CH_I_LOAD     ADC_CHANNEL_3
#define ADC_CH_NTC        ADC_CHANNEL_4

// ---- Power stage: pins with no pull and no activity at reset. Add a 10k pull-down at each one ----
#define PIN_GATE_A        GPIO_NUM_0   // -> R32 -> U4 TLP350 LED (no pull at reset)
#define PIN_GATE_B        GPIO_NUM_6   // -> R31 -> U5 TLP350 LED (drives Q10/12/13/15). Gives up ADC1_CH5
#define PIN_LOAD_EN       GPIO_NUM_23  // load output switch (Q2/Q3 path); bottom pad, no pull at reset
#define PIN_FAN           GPIO_NUM_11  // -> R34 4.7k -> QF1 SS8050 base. UART0 TX: the boot log only blips the fan

// ---- TFT GMT130-V1.0 (has no CS -> SPI mode 3), four adjacent edge pins ----
#define PIN_TFT_SDA       GPIO_NUM_7   // MOSI. Strap (JTAG select, only read if the eFuse is set); add 10k to GND
#define PIN_TFT_SCK       GPIO_NUM_8
#define PIN_TFT_DC        GPIO_NUM_9
#define PIN_TFT_BLK       GPIO_NUM_10  // LEDC PWM dimming
#define PIN_TFT_RES       GPIO_NUM_NC  // RES -> 10k to 3V3 + 100nF to GND (power-on reset); firmware also sends SWRESET
#define TFT_SPI_HZ        (40 * 1000 * 1000)  // via the GPIO matrix. ST7789 write cycle is 16 ns min (62.5 MHz)

// ---- Buttons: active low to GND; internal pull-up plus an external 10k on the long wires ----
#define PIN_BTN_ESC       GPIO_NUM_28  // PRG/ESC. Boot strap: holding it during reset = download mode
#define PIN_BTN_UP        GPIO_NUM_25  // SDIO-edge strap only, harmless when pulled up
#define PIN_BTN_DOWN      GPIO_NUM_24
#define PIN_BTN_ENTER     GPIO_NUM_12  // UART0 RX, has a weak pull-up from reset, a good fit for a button
