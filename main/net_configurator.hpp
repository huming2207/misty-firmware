#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/timers.h>

#include <esp_err.h>


#include "config_server.hpp"
#include "esp_wifi_types_generic.h"
#include "nvs.h"

ESP_EVENT_DECLARE_BASE(NET_CFG_EVENTS);

class net_configurator
{
public:
    static net_configurator &instance()
    {
        static net_configurator _instance;
        return _instance;
    }

    void operator=(net_configurator const &) = delete;
    net_configurator(net_configurator const &) = delete;

    enum net_events : uint32_t
    {
        NET_CFG_EVENT_FORCE_WIFI_STOP = 0,
        NET_CFG_EVENT_WIFI_START_MANUAL = 1, // Turn on WiFi for 10 minutes for user to modify configurations - after factory reset it's in AP mode, otherwise STA mode
        NET_CFG_EVENT_WIFI_START_SYNC = 2, // Turn on WiFi in STA mode, get NTP time and weather info synced and then turn off WiFi
        NET_CFG_EVENT_WIFI_SYNC_DONE = 3,
    };

    enum net_states : uint32_t
    {
        NET_CFG_STATE_WIFI_AP_ENABLED = BIT(0),
        NET_CFG_STATE_WIFI_STA_ENABLED = BIT(1),
        NET_CFG_STATE_WIFI_ENABLED = (NET_CFG_STATE_WIFI_AP_ENABLED | NET_CFG_STATE_WIFI_STA_ENABLED),
        NET_CFG_STATE_GOT_IP = BIT(2),
        NET_CFG_STATE_SYNC_DONE = BIT(3),
    };

public:
    esp_err_t init();
    esp_err_t load_wifi();
    esp_err_t set_wifi_config(wifi_config_t *config);
    esp_err_t nuke_config();

private:
    net_configurator() = default;
    static bool wifi_has_station_config();
    static void wifi_evt_handler(void *_ctx, esp_event_base_t evt_base, int32_t evt_id, void *evt_data);
    static void net_cfg_evt_handler(void *_ctx, esp_event_base_t evt_base, int32_t evt_id, void *evt_data);
    static void wifi_off_timer_cb(TimerHandle_t timer);
    static void wifi_sync_timer_cb(TimerHandle_t timer);
    static void sntp_sync_cb(timeval *tv);

    nvs_handle_t nvs = 0; // Not to be confused with scheduler's NVS - this is for WiFi and network
    uint32_t retry_cnt = 0;
    EventGroupHandle_t net_events = nullptr;
    TimerHandle_t wifi_off_timer = nullptr;
    TimerHandle_t wifi_sync_timer = nullptr;
    config_server server = {};
    static constexpr uint32_t MAX_RETRY_COUNT = 5;
    static constexpr uint32_t WIFI_MANUAL_ENABLE_TIMEOUT_TICKS = pdMS_TO_TICKS(600*1000); // 10 minutes
    static constexpr uint32_t WIFI_SYNC_PERIOD_TICKS = pdMS_TO_TICKS(7200*1000); // 120 minutes
    static constexpr char TAG[] = "net_config";
};
