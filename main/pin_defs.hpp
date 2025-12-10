#pragma once

#include <driver/gpio.h>

namespace misty
{
    static constexpr gpio_num_t I2C_SDA_PIN = GPIO_NUM_4;
    static constexpr gpio_num_t I2C_SCL_PIN = GPIO_NUM_5;
    static constexpr gpio_num_t TS_DRDY_PIN = GPIO_NUM_6;
    static constexpr gpio_num_t PUMP_SLEEP_PIN = GPIO_NUM_18;
    static constexpr gpio_num_t PUMP_FAULT_PIN = GPIO_NUM_19;
    static constexpr gpio_num_t PUMP_AIN1_PIN = GPIO_NUM_20;
    static constexpr gpio_num_t PUMP_AIN2_PIN = GPIO_NUM_21;
    static constexpr gpio_num_t PUMP_BIN1_PIN = GPIO_NUM_22;
    static constexpr gpio_num_t PUMP_BIN2_PIN = GPIO_NUM_23;
}