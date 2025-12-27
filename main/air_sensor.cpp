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

    for (size_t idx = 0; idx < MEAS_SLOTS; idx += 1) {
        temp_slots[idx] = -300; // Ambient temperature can't be lower than -273degC anyway
        humid_slots[idx] = -1; // RH is % so it can't be negative anyway
    }

    measure_timer = xTimerCreate("air_sense", pdMS_TO_TICKS(MEAS_WINDOW_INTERVAL_MINUTE * 60000UL), pdTRUE, this, sense_timer_cb);
    if (measure_timer == nullptr) {
        ESP_LOGE(TAG, "Failed to create air sensor timer");
        return ESP_ERR_NO_MEM;
    }

    measure_evt = xEventGroupCreate();
    if (measure_evt == nullptr) {
        ESP_LOGE(TAG, "Failed to create air sensor event group");
        return ESP_ERR_NO_MEM;
    }

    if (xTaskCreate(sense_process_task, "air_sense_tsk", 4096, this, tskIDLE_PRIORITY + 3, nullptr) == pdFAIL) {
        ESP_LOGE(TAG, "Failed to create air sensor task");
        return ESP_ERR_NO_MEM;
    }

    if (xTimerStart(measure_timer, 1) == pdFAIL) {
        ESP_LOGE(TAG, "Failed to start air sensor timer");
        return ESP_ERR_INVALID_STATE;
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
        ESP_LOGW(TAG, "sense: Write average value to slot %d", (int)history_slot_idx);
        humid_slots[history_slot_idx] = (humid_accumulator / (float)accumulated_reading_cnt);
        temp_slots[history_slot_idx] = (temp_accumulator / (float)accumulated_reading_cnt);

        history_slot_idx += 1;

        if (history_slot_idx >= MEAS_SLOTS) {
            history_slot_idx = 0;
        }

        accumulated_reading_cnt = 0;
        temp_accumulator = 0;
        humid_accumulator = 0;
    }

    float temperature_sum = 0, humidity_sum = 0;
    size_t valid_count = 0;

    // Sum up all valid slots to calculate the rolling average
    for (size_t idx = 0; idx < MEAS_SLOTS; idx += 1) {
        // Check if the slot has valid data (initialized to -300/-1)
        if (temp_slots[idx] > -273.0f && humid_slots[idx] >= 0) {
            temperature_sum += temp_slots[idx];
            humidity_sum += humid_slots[idx];
            valid_count++;
        }
    }

    // Include the current partial accumulation in the average if it exists
    if (accumulated_reading_cnt > 0) {
        temperature_sum += (temp_accumulator / (float)accumulated_reading_cnt);
        humidity_sum += (humid_accumulator / (float)accumulated_reading_cnt);
        valid_count++;
    }

    if (valid_count > 0) {
        latest_humidity_avg = humidity_sum / (float)valid_count;
        latest_temperature_avg = temperature_sum / (float)valid_count;
        xEventGroupSetBits(measure_evt, HAS_VALID_DATA);
    }

    ESP_LOGI(TAG, "sense: avg temp=%.3f, humid=%.3f", latest_temperature_avg.load(), latest_humidity_avg.load());
    return ESP_OK;
}

bool air_sensor::has_valid_reading() const
{
    if (measure_evt == nullptr) {
        return false;
    }

    return (xEventGroupGetBits(measure_evt) & HAS_VALID_DATA) != 0;
}

float air_sensor::average_temperature() const
{
    return latest_temperature_avg;
}

float air_sensor::average_humidity() const
{
    return latest_humidity_avg;
}

void air_sensor::sense_timer_cb(TimerHandle_t timer)
{
    auto *ctx = (air_sensor *)pvTimerGetTimerID(timer);
    assert(ctx != nullptr);

    xEventGroupSetBits(ctx->measure_evt, READY_TO_READ);
}

void air_sensor::sense_process_task(void* _ctx)
{
    auto *ctx = (air_sensor *)_ctx;
    while (true) {
        xEventGroupWaitBits(ctx->measure_evt, READY_TO_READ, pdTRUE, pdTRUE, portMAX_DELAY);
        ctx->sense();
        vTaskDelay(1);
    }
}
