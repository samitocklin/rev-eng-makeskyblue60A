#include <string.h>
#include <stdio.h>
#include "net.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "nvs.h"

#define CONNECTED_BIT BIT0

static const char *TAG = "net";
static EventGroupHandle_t s_bits;
static esp_netif_t *s_netif;
static bool s_started;
static char s_ip[16] = "";

static void on_wifi(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_bits, CONNECTED_BIT);
        s_ip[0] = 0;
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = data;
        snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&e->ip_info.ip));
        xEventGroupSetBits(s_bits, CONNECTED_BIT);
        ESP_LOGI(TAG, "connected, ip %s", s_ip);
    }
}

static void load_creds(char *ssid, size_t n, char *pass, size_t m)
{
    ssid[0] = pass[0] = 0;
    nvs_handle_t h;
    if (nvs_open("msb60", NVS_READONLY, &h) != ESP_OK) return;
    size_t len = n;
    nvs_get_str(h, "ssid", ssid, &len);
    len = m;
    nvs_get_str(h, "pass", pass, &len);
    nvs_close(h);
}

static void start(void)
{
    char ssid[33], pass[65];
    load_creds(ssid, sizeof(ssid), pass, sizeof(pass));
    if (!ssid[0]) { ESP_LOGI(TAG, "no Wi-Fi credentials, radio off"); return; }

    if (!s_started) {
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &on_wifi, NULL, NULL));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_wifi, NULL, NULL));
        s_netif = esp_netif_create_default_wifi_sta();
        s_started = true;
    }
    wifi_config_t wc = { 0 };
    strlcpy((char *)wc.sta.ssid, ssid, sizeof(wc.sta.ssid));
    strlcpy((char *)wc.sta.password, pass, sizeof(wc.sta.password));
    wc.sta.threshold.authmode = pass[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "connecting to '%s'", ssid);
}

void net_init(void)
{
    s_bits = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    start();
}

void net_set_credentials(const char *ssid, const char *pass)
{
    nvs_handle_t h;
    if (nvs_open("msb60", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, "ssid", ssid);
        nvs_set_str(h, "pass", pass);
        nvs_commit(h);
        nvs_close(h);
    }
    if (s_started) esp_wifi_stop();
    start();
}

void net_disable(void)
{
    net_set_credentials("", "");
}

bool net_connected(void)
{
    return s_bits && (xEventGroupGetBits(s_bits) & CONNECTED_BIT);
}

const char *net_ip(void) { return s_ip; }

const char *net_ssid(void)
{
    static char ssid[33];
    wifi_config_t cfg;
    if (s_started && esp_wifi_get_config(WIFI_IF_STA, &cfg) == ESP_OK) {
        strlcpy(ssid, (char *)cfg.sta.ssid, sizeof(ssid));
        return ssid;
    }
    return "";
}

int net_rssi(void)
{
    wifi_ap_record_t r;
    if (s_started && esp_wifi_sta_get_ap_info(&r) == ESP_OK) return r.rssi;
    return 0;
}
