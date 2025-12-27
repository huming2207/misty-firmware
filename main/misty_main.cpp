#include "esp_log.h"
#include <esp_err.h>
#include <nvs.h>
#include <nvs_flash.h>

#include "air_sensor.hpp"
#include "net_configurator.hpp"
#include "sched_manager.hpp"

#define TAG "main"

extern "C" void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_LOGI(TAG, "Init'ing NVS");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "Config loaded");

    ESP_ERROR_CHECK(air_sensor::instance().init());
    ESP_LOGI(TAG, "Sensor loaded");

    ESP_ERROR_CHECK(net_configurator::instance().init());
    ESP_LOGI(TAG, "Net config loaded");

    ESP_ERROR_CHECK(sched_manager::instance().init());
    ESP_LOGI(TAG, "Schedule manager loaded");
}
