#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_netif_sntp.h>
#include "net_configurator.hpp"

#include <esp_mac.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

ESP_EVENT_DEFINE_BASE(NET_CFG_EVENTS);

esp_err_t net_configurator::init()
{
    esp_err_t ret = nvs_open("net", NVS_READWRITE, &nvs);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Can't load NVS namespace: 0x%x", ret);
        return ret;
    }

    uint32_t inited = 0;
    ret = nvs_get_u32(nvs, NVS_KEY_INITIALISED, &inited);
    if (ret == ESP_ERR_NVS_NOT_FOUND || ret == ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG, "init: First boot, set initialised");
        ret = nvs_set_u32(nvs, NVS_KEY_INITIALISED, INITIALISED_MAGIC_VALUE);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "init: can't set INIT flag: 0x%x", ret);
        }
    }

    if (ret == ESP_OK && inited != INITIALISED_MAGIC_VALUE) {
        ESP_LOGE(TAG, "init: factory reset requested, erase & restart soon!");
        vTaskDelay(pdMS_TO_TICKS(10000));
        nvs_flash_erase();
        esp_restart();
    }

    if (ret == ESP_OK && inited == INITIALISED_MAGIC_VALUE) {
        ESP_LOGI(TAG, "init: LGTM");
    } else {
        ESP_LOGE(TAG, "init: something went wrong: 0x%x", ret);
        return ret;
    }

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    esp_sntp_config_t sntp_cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(2, ESP_SNTP_SERVER_LIST("pool.ntp.org", "time.apple.com"));
    sntp_cfg.smooth_sync = false; // We want time sync as soon as possible
    ret = esp_netif_sntp_init(&sntp_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "init: can't start SNTP");
        return ret;
    }

    return ESP_OK;
}

esp_err_t net_configurator::load_wifi()
{
    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_wifi_init(&init_cfg);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "load_wifi: can't init WiFi driver: 0x%x", ret);
        return ret;
    }

    wifi_config_t wifi_cfg = {};
    ret = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_evt_handler, this, nullptr);
    ret = ret ?: esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_evt_handler, this, nullptr);
    ret = ret ?: esp_event_handler_instance_register(NET_CFG_EVENTS, ESP_EVENT_ANY_ID, &wifi_evt_handler, this, nullptr);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "load_wifi: can't register event??? ret=0x%x", ret);
        // Should we quit here???
    }

    ret = esp_wifi_get_config(WIFI_IF_STA, &wifi_cfg);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "load_wifi: can't get config??? ret=0x%x", ret);
        // Should we quit here???
    }

    if (init_cfg.magic != WIFI_INIT_CONFIG_MAGIC || strlen((char *)wifi_cfg.sta.ssid) == 0) {
        ESP_LOGW(TAG, "load_wifi: invalid STA, starting AP now");

        uint8_t mac_addr[6] = { 0 };
        esp_read_mac(mac_addr, ESP_MAC_WIFI_STA);
        int len = snprintf((char *)wifi_cfg.ap.ssid, sizeof(wifi_cfg.ap.ssid), "misty-%02x%02x%02x", mac_addr[3], mac_addr[4], mac_addr[5]);
        if (len < 5) {
            ESP_LOGW(TAG, "load_wifi: failed to concat WiFi AP SSID (might be bug?)");
            strlcpy((char *)wifi_cfg.ap.ssid, "misty", 32);
        } else {
            wifi_cfg.ap.ssid_len = (uint8_t)len;
        }

        wifi_cfg.ap.authmode = WIFI_AUTH_OPEN;
        wifi_cfg.ap.ssid_hidden = 0;

        ret = esp_wifi_set_mode(WIFI_MODE_APSTA);
        ret = ret ?: esp_wifi_set_config(WIFI_IF_AP, &wifi_cfg);
        ret = ret ?: esp_wifi_start();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "load_wifi: can't start WiFi AP: 0x%x", ret);
            return ret;
        }

        ESP_LOGI(TAG, "load_wifi: WiFi AP started");
    } else {
        ESP_LOGI(TAG, "load_wifi: has valid config for SSID %s", wifi_cfg.sta.ssid);
        ret = esp_wifi_set_mode(WIFI_MODE_STA);
        ret = ret ?: esp_wifi_start();
        ret = ret ?: esp_wifi_set_inactive_time(WIFI_IF_STA, 6); // Might tune this later, see https://github.com/espressif/esp-idf/blob/v5.5.1/examples/wifi/power_save/main/Kconfig.projbuild#L35
        ret = ret ?: esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
    }

    return ret;
}

esp_err_t net_configurator::set_wifi_config(wifi_config_t* config)
{
    return esp_wifi_set_config(WIFI_IF_STA, config);
}

void net_configurator::wifi_evt_handler(void* _ctx, esp_event_base_t evt_base, int32_t evt_id, void* evt_data)
{
    auto *ctx = (net_configurator *)_ctx;
    if (evt_base == WIFI_EVENT) {
        switch (evt_id) {
            case WIFI_EVENT_STA_DISCONNECTED:
            case WIFI_EVENT_STA_START: {
                if (ctx->retry_cnt < MAX_RETRY_COUNT) {
                    esp_wifi_connect();
                }
                ctx->retry_cnt += 1;
                break;
            }

            case WIFI_EVENT_STA_CONNECTED: {
                ESP_LOGI(TAG, "WiFi connected");
                break;
            }

            case WIFI_EVENT_STA_BEACON_OFFSET_UNSTABLE: {
                auto *event = (wifi_event_sta_beacon_offset_unstable_t*)evt_data;
                ESP_LOGI(TAG, "WiFi beacon sample unstable, success rate %.4f", event->beacon_success_rate);
                esp_wifi_beacon_offset_sample_beacon();
                break;
            }

            default: {
                ESP_LOGD(TAG, "Unhandled WiFi event: 0x%lx", evt_id);
                break;
            }
        }
    } else if (evt_base == IP_EVENT && evt_id == IP_EVENT_STA_GOT_IP) {
        auto *event = (ip_event_got_ip_t*)evt_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Got IP Gateway: " IPSTR, IP2STR(&event->ip_info.gw));
    } else if (evt_base == NET_CFG_EVENTS && evt_id == NET_CFG_EVENT_FORCE_WIFI_STOP) {
        ESP_LOGI(TAG, "WiFi stop requested");
        esp_wifi_stop();
    }
}

