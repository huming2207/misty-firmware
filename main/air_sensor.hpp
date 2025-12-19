#pragma once

#include <esp_err.h>

#include "drv8833.hpp"
#include "hdc2080.hpp"

#include "pin_defs.hpp"

class air_sensor
{
public:
    static air_sensor &instance()
    {
        static air_sensor _instance;
        return _instance;
    }

    void operator=(air_sensor const &) = delete;
    air_sensor(air_sensor const &) = delete;

private:
    air_sensor() = default;
    static constexpr size_t MEAS_WINDOW_HOURS = 24;
    static constexpr size_t MEAS_WINDOW_INTERVAL_MINUTE = 30;
    static constexpr size_t MEAS_ACCUM_COUNT = 5; // Every MEAS_WINDOW_INTERVAL_MINUTE measure MEAS_ACCUM_COUNT times
    static constexpr size_t MEASURE_INTERVAL_MINUTE = MEAS_WINDOW_INTERVAL_MINUTE / MEAS_ACCUM_COUNT;
    static constexpr size_t MEAS_SLOTS = (MEAS_WINDOW_HOURS * 60) / MEAS_WINDOW_INTERVAL_MINUTE;
    static constexpr char TAG[] = "air_sensor";


public:
    esp_err_t init();
    esp_err_t sense();
    [[nodiscard]] float average_temperature() const;
    [[nodiscard]] float average_humidity() const;

private:
    // One-hour accumulator
    uint8_t accumulated_reading_cnt = 0;
    size_t history_slot_idx = 0;
    size_t valid_slots_count = 0;
    float temp_accumulator = 0;
    float humid_accumulator = 0;
    float latest_temperature_avg = 0;
    float latest_humidity_avg = 0;
    float temp_slots[MEAS_SLOTS] = {};
    float humid_slots[MEAS_SLOTS] = {};


    hdc2080 temp_sensor = hdc2080();
    drv8833 pump = drv8833(misty::PUMP_AIN1_PIN, misty::PUMP_AIN2_PIN, misty::PUMP_BIN1_PIN, misty::PUMP_BIN2_PIN,
                            misty::PUMP_FAULT_PIN, misty::PUMP_SLEEP_PIN);


};
