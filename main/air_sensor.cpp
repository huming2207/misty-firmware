#include <esp_log.h>

#include "air_sensor.hpp"

esp_err_t air_sensor::init()
{
    esp_err_t ret = temp_sensor.init(misty::TS_DRDY_PIN, misty::I2C_SDA_PIN, misty::I2C_SCL_PIN);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "init: can't init temperature sensor: 0x%x", ret);
        return ret;
    }

    ret = ret ?: pump.init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "init: can't init pump driver: 0x%x", ret);
        return ret;
    }

    return ESP_OK;
}
