idf_component_register(SRCS "main.c" "settings.c" "sense.c" "buttons.c" "power.c" "display.c" "ui.c" "console.c"
                              "net.c" "telemetry.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_driver_gpio esp_driver_spi esp_driver_ledc esp_lcd esp_adc
                                nvs_flash console esp_timer esp_wifi esp_netif esp_event
                                mqtt esp_http_server)
