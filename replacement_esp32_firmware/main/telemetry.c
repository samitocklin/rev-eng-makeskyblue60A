#include <stdio.h>
#include <string.h>
#include "telemetry.h"
#include "app.h"
#include "sense.h"
#include "power.h"
#include "settings.h"
#include "net.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt_client.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "telem";
static esp_mqtt_client_handle_t s_mqtt;
static char s_uri[128], s_topic[64];
static volatile bool s_mqtt_ok;

void telemetry_json(char *buf, size_t n)
{
    meas_t m;
    sense_get(&m);
    snprintf(buf, n,
        "{\"pv_v\":%.2f,\"bat_v\":%.2f,\"chg_a\":%.2f,\"load_a\":%.2f,"
        "\"out_w\":%.1f,\"temp_c\":%.1f,\"temp_ok\":%s,\"state\":\"%s\","
        "\"duty\":%.3f,\"fault\":\"%s\",\"load_on\":%s,\"fan_on\":%s,"
        "\"stage\":%s,\"cal\":%lu}",
        m.pv_v, m.bat_v, m.chg_a, m.load_a, m.out_w, m.temp_c,
        m.temp_ok ? "true" : "false", power_state_name(power_state()),
        power_duty(), power_fault(), power_load_on() ? "true" : "false",
        power_fan_on() ? "true" : "false", power_stage_enabled() ? "true" : "false",
        (unsigned long)g_set.cal_mask);
}

static void mqtt_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    switch ((esp_mqtt_event_id_t)id) {
    case MQTT_EVENT_CONNECTED:
        s_mqtt_ok = true;
        ESP_LOGI(TAG, "mqtt connected");
        break;
    case MQTT_EVENT_DISCONNECTED:
        s_mqtt_ok = false;
        break;
    default:
        break;
    }
}

static void load_broker(void)
{
    s_uri[0] = s_topic[0] = 0;
    nvs_handle_t h;
    if (nvs_open("msb60", NVS_READONLY, &h) != ESP_OK) return;
    size_t len = sizeof(s_uri);
    nvs_get_str(h, "mqtt_uri", s_uri, &len);
    len = sizeof(s_topic);
    if (nvs_get_str(h, "mqtt_topic", s_topic, &len) != ESP_OK || !s_topic[0])
        strlcpy(s_topic, "msb60/status", sizeof(s_topic));
    nvs_close(h);
}

static void mqtt_start(void)
{
    if (s_mqtt) { esp_mqtt_client_destroy(s_mqtt); s_mqtt = NULL; s_mqtt_ok = false; }
    if (!s_uri[0]) { ESP_LOGI(TAG, "mqtt off (no broker configured)"); return; }
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = s_uri,
        .network.timeout_ms = 5000,
        .session.keepalive = 30,
    };
    s_mqtt = esp_mqtt_client_init(&cfg);
    if (!s_mqtt) { ESP_LOGE(TAG, "mqtt init failed"); return; }
    esp_mqtt_client_register_event(s_mqtt, ESP_EVENT_ANY_ID, mqtt_event, NULL);
    esp_mqtt_client_start(s_mqtt);
    ESP_LOGI(TAG, "mqtt -> %s topic %s", s_uri, s_topic);
}

static void pub_task(void *arg)
{
    char json[512];
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        if (!s_mqtt || !net_connected()) continue;
        telemetry_json(json, sizeof(json));
        esp_mqtt_client_publish(s_mqtt, s_topic, json, 0, 0, 0);
    }
}

static esp_err_t status_get(httpd_req_t *req)
{
    char json[512];
    telemetry_json(json, sizeof(json));
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static void http_start(void)
{
    httpd_handle_t srv;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.lru_purge_enable = true;
    if (httpd_start(&srv, &cfg) != ESP_OK) { ESP_LOGE(TAG, "httpd start failed"); return; }
    httpd_uri_t uri = { .uri = "/status", .method = HTTP_GET, .handler = status_get };
    httpd_register_uri_handler(srv, &uri);
    ESP_LOGI(TAG, "http /status ready");
}

void telemetry_init(void)
{
    load_broker();
    mqtt_start();
    http_start();
    xTaskCreate(pub_task, "telem", 3072, NULL, 4, NULL);
}

bool telemetry_mqtt_ok(void) { return s_mqtt_ok; }

const char *telemetry_topic(void) { return s_topic; }

void telemetry_set_broker(const char *uri, const char *topic)
{
    nvs_handle_t h;
    if (nvs_open("msb60", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, "mqtt_uri", uri);
        if (topic && topic[0]) nvs_set_str(h, "mqtt_topic", topic);
        nvs_commit(h);
        nvs_close(h);
    }
    strlcpy(s_uri, uri, sizeof(s_uri));
    if (topic && topic[0]) strlcpy(s_topic, topic, sizeof(s_topic));
    else if (!s_topic[0]) strlcpy(s_topic, "msb60/status", sizeof(s_topic));
    mqtt_start();
}
