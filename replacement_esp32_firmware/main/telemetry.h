// Telemetry: MQTT JSON publish + HTTP /status JSON
#pragma once
#include <stddef.h>
#include <stdbool.h>

void telemetry_init(void);
bool telemetry_mqtt_ok(void);
void telemetry_set_broker(const char *uri, const char *topic);  // "" uri = off
const char *telemetry_topic(void);
void telemetry_json(char *buf, size_t n);   // shared payload builder
