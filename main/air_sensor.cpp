#include <esp_log.h>

#include "air_sensor.hpp"

esp_err_t air_sensor::init()
{
    esp_err_t ret = temp_sensor.init(misty::TS_DRDY_PIN, misty::I2C_SDA_PIN, misty::I2C_SCL_PIN);
    ret = ret ?: temp_sensor.reset();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "init: can't init temperature sensor: 0x%x", ret);
        return ret;
    }


    ret = ret ?: pump.init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "init: can't init pump driver: 0x%x", ret);
        return ret;
    }

    for (size_t idx = 0; idx < MEAS_SLOTS; idx += 1) {
        temp_slots[idx] = -300; // Ambient temperature can't be lower than -273degC anyway
        humid_slots[idx] = -1; // RH is % so it can't be negative anyway
    }

    return ESP_OK;
}

esp_err_t air_sensor::sense()
{
    float temperature = 0, humidity = 0;
    auto ret = temp_sensor.set_measure_config(true);
    vTaskDelay(1);
    ret = ret ?: temp_sensor.read_temperature(temperature);
    ret = ret ?: temp_sensor.read_humidity(humidity);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "sense: update reading failed: 0x%x", ret);
        return ret;
    }

    temp_accumulator += temperature;
    humid_accumulator += humidity;
    accumulated_reading_cnt += 1;

    if (accumulated_reading_cnt >= MEAS_ACCUM_COUNT) {
        ESP_LOGW(TAG, "sense: Write average value to slot now");
        humid_slots[history_slot_idx] = (humid_accumulator / (float)accumulated_reading_cnt);
        temp_slots[history_slot_idx] = (temp_accumulator / (float)accumulated_reading_cnt);

        history_slot_idx += 1;

        if (history_slot_idx >= MEAS_SLOTS) {
            history_slot_idx = 0;
            latest_temperature_avg = (temp_accumulator / (float)accumulated_reading_cnt);
            latest_humidity_avg = (humid_accumulator / (float)accumulated_reading_cnt);
        }

        float temperature_sum = 0, humidity_sum = 0;

        // For the first round the  slots will be set to invalid temperature/humidity - do not add them in
        for (size_t idx = 0; idx < MEAS_SLOTS; idx += 1) {
            temperature_sum += temp_slots[idx] < -273.0f ? 0 : temp_slots[idx];
            humidity_sum += humid_slots[idx] < 0 ? 0 : humid_slots[idx];
        }

        latest_humidity_avg = humidity_sum / (float)history_slot_idx;
        latest_temperature_avg = temperature_sum / (float)history_slot_idx;

        accumulated_reading_cnt = 0;
        temp_accumulator = 0;
        humid_accumulator = 0;
    }

    return ESP_OK;
}
