#pragma once

#include <esp_err.h>

#include "esp_event_base.h"
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
    };

public:
    esp_err_t init();
    esp_err_t load_wifi();
    esp_err_t set_wifi_config(wifi_config_t *config);

private:
    net_configurator() = default;
    static void wifi_evt_handler(void *_ctx, esp_event_base_t evt_base, int32_t evt_id, void *evt_data);

private:
    nvs_handle_t nvs = 0; // Not to be confused with scheduler's NVS - this is for WiFi and network
    uint32_t retry_cnt = 0;
    EventGroupHandle_t net_events = nullptr;
    static constexpr uint32_t MAX_RETRY_COUNT = 5;
    static constexpr uint32_t INITIALISED_MAGIC_VALUE = 1145141919UL;
    static constexpr char TAG[] = "net_config";
    static constexpr char NVS_KEY_INITIALISED[] = "inited";
    static constexpr char NVS_KEY_WIFI_AP_NAME[] = "wifi_ap";
    static constexpr char NVS_KEY_WIFI_AP_PASSWD[] = "wifi_passwd";
};
