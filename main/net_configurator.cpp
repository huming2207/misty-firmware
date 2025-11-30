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
    net_events = xEventGroupCreate();
    if (net_events == nullptr) {
        ESP_LOGE(TAG, "Failed to create net config event group");
        return ESP_ERR_NO_MEM;
    }

    wifi_off_timer = xTimerCreate("net_manual_off", WIFI_MANUAL_ENABLE_TIMEOUT_TICKS, pdFALSE, this, wifi_off_timer_cb);
    if (wifi_off_timer == nullptr) {
        ESP_LOGE(TAG, "Failed to create WiFi off timer");
        return ESP_ERR_NO_MEM;
    }

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    esp_err_t ret = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_evt_handler, this, nullptr);
    ret = ret ?: esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_evt_handler, this, nullptr);
    ret = ret ?: esp_event_handler_instance_register(NET_CFG_EVENTS, ESP_EVENT_ANY_ID, &wifi_evt_handler, this, nullptr);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "init: can't register event??? ret=0x%x", ret);
        // Should we quit here???
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
        xEventGroupSetBits(net_events, NET_CFG_STATE_WIFI_AP_ENABLED);
    } else {
        ESP_LOGI(TAG, "load_wifi: has valid config for SSID %s", wifi_cfg.sta.ssid);
        ret = esp_wifi_set_mode(WIFI_MODE_STA);
        ret = ret ?: esp_wifi_start();
        ret = ret ?: esp_wifi_set_inactive_time(WIFI_IF_STA, 6); // Might tune this later, see https://github.com/espressif/esp-idf/blob/v5.5.1/examples/wifi/power_save/main/Kconfig.projbuild#L35
        ret = ret ?: esp_wifi_set_ps(WIFI_PS_MAX_MODEM);

        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "load_wifi: WiFi STA start failed: 0x%x", ret);
            return ret;
        }

        ESP_LOGI(TAG, "load_wifi: WiFi STA started");
        xEventGroupSetBits(net_events, NET_CFG_STATE_WIFI_STA_ENABLED);
    }

    return ret;
}

esp_err_t net_configurator::set_wifi_config(wifi_config_t* config)
{
    ESP_LOGI(TAG, "Got new WiFi config!");
    return esp_wifi_set_config(WIFI_IF_STA, config);
}

void net_configurator::wifi_evt_handler(void* _ctx, esp_event_base_t evt_base, int32_t evt_id, void* evt_data)
{
    esp_err_t ret = ESP_OK;
    auto *ctx = (net_configurator *)_ctx;
    if (evt_base == WIFI_EVENT) {
        switch (evt_id) {
            case WIFI_EVENT_STA_DISCONNECTED:
            case WIFI_EVENT_STA_START: {
                xEventGroupClearBits(ctx->net_events, NET_CFG_STATE_GOT_IP);
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

        xEventGroupSetBits(ctx->net_events, NET_CFG_STATE_GOT_IP);
        esp_sntp_config_t sntp_cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(2, ESP_SNTP_SERVER_LIST("pool.ntp.org", "time.apple.com"));
        sntp_cfg.smooth_sync = false; // We want time sync as soon as possible
        sntp_cfg.sync_cb = sntp_sync_cb;
        ret = esp_netif_sntp_init(&sntp_cfg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Can't start SNTP: 0x%x", ret);
            esp_event_post(NET_CFG_EVENTS, NET_CFG_EVENT_FORCE_WIFI_STOP, nullptr, 0, pdMS_TO_TICKS(3000));
        }
    }
}

void net_configurator::net_cfg_evt_handler(void* _ctx, esp_event_base_t evt_base, int32_t evt_id, void* evt_data)
{
    esp_err_t ret = ESP_OK;
    auto *ctx = (net_configurator *)_ctx;

    switch (evt_id) {
        case NET_CFG_EVENT_FORCE_WIFI_STOP: {
            ESP_LOGI(TAG, "WiFi stop requested");
            esp_wifi_stop();
            break;
        }

        case NET_CFG_EVENT_WIFI_START_MANUAL: {
            ESP_LOGI(TAG, "WiFi start - manual");
            ret = ctx->load_wifi();
            if (ret != ESP_OK) {
                ESP_LOGI(TAG, "WiFi start failed: 0x%x", ret);
            }

            if (xTimerIsTimerActive(ctx->wifi_off_timer) != pdFALSE) {
                if (xTimerStop(ctx->wifi_off_timer, pdMS_TO_TICKS(3000)) != pdPASS) {
                    ESP_LOGE(TAG, "Can't stop previous WiFi timer!");
                    esp_event_post(NET_CFG_EVENTS, NET_CFG_EVENT_FORCE_WIFI_STOP, nullptr, 0, pdMS_TO_TICKS(3000));
                }
            }

            if (xTimerStart(ctx->wifi_off_timer, pdMS_TO_TICKS(3000)) != pdPASS) {
                ESP_LOGE(TAG, "Can't start WiFi timer!");
                esp_event_post(NET_CFG_EVENTS, NET_CFG_EVENT_FORCE_WIFI_STOP, nullptr, 0, pdMS_TO_TICKS(3000));
            }

            break;
        }
        case NET_CFG_EVENT_WIFI_START_SYNC: {
            ESP_LOGI(TAG, "WiFi start - auto sync");
            ret = ctx->load_wifi();
            if (ret != ESP_OK) {
                ESP_LOGI(TAG, "WiFi start failed: 0x%x", ret);
                esp_event_post(NET_CFG_EVENTS, NET_CFG_EVENT_FORCE_WIFI_STOP, nullptr, 0, pdMS_TO_TICKS(3000));
            }

            break;
        }
        case NET_CFG_EVENT_WIFI_SYNC_DONE: {
            ESP_LOGW(TAG, "WIFI Sync done");
            xEventGroupSetBits(ctx->net_events, NET_CFG_STATE_SYNC_DONE);
            esp_wifi_stop(); // Just stop for now
            break;
        }
        default: {
            ESP_LOGW(TAG, "Unhandled net configurator event %ld", evt_id);
            break;
        }
    }
}

void net_configurator::wifi_off_timer_cb(TimerHandle_t timer)
{
    esp_event_post(NET_CFG_EVENTS, NET_CFG_EVENT_FORCE_WIFI_STOP, nullptr, 0, pdMS_TO_TICKS(1000));
}

void net_configurator::sntp_sync_cb(timeval* tv)
{
    esp_event_post(NET_CFG_EVENTS, NET_CFG_EVENT_WIFI_SYNC_DONE, nullptr, 0, pdMS_TO_TICKS(3000));
}
