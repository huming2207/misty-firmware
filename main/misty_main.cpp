#include "esp_log.h"
#include <esp_err.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <hal/gpio_ll.h>

#include "air_sensor.hpp"
#include "net_configurator.hpp"
#include "pump_manager.hpp"
#include "sched_manager.hpp"
#include "pin_defs.hpp"

#define TAG "main"

extern "C" void app_main(void)
{
    if (gpio_ll_get_level(&GPIO, misty::PUMP_TRIG_BTN_PIN) == 0 && gpio_ll_get_level(&GPIO, misty::CONFIG_BTN_PIN) == 0) {
        vTaskDelay(100); // Dumb way of de-glitching & long press detection
        if (gpio_ll_get_level(&GPIO, misty::PUMP_TRIG_BTN_PIN) == 0 && gpio_ll_get_level(&GPIO, misty::CONFIG_BTN_PIN) == 0) {
            ESP_LOGW(TAG, "Factory reset detected, proceeding");
            nvs_flash_erase();
            ESP_LOGW(TAG, "Factory reset done!!");
        }
    }

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

    ESP_ERROR_CHECK(misty::setup_input_interrupts());
    ESP_ERROR_CHECK(pump_manager::instance().init());
    ESP_LOGI(TAG, "Pump loaded");

    ESP_ERROR_CHECK(sched_manager::instance().init());
    ESP_LOGI(TAG, "Schedule manager loaded");
}
