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
    hdc2080 temp_sensor = hdc2080();
    drv8833 pump = drv8833(misty::PUMP_AIN1_PIN, misty::PUMP_AIN2_PIN, misty::PUMP_BIN1_PIN, misty::PUMP_BIN2_PIN,
                            misty::PUMP_FAULT_PIN, misty::PUMP_SLEEP_PIN);

public:
    esp_err_t init();

private:
    static constexpr char TAG[] = "air_sensor";
};
