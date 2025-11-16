#pragma once

#include "driver/gpio.h"
#include "driver/ledc.h"

class drv8833
{
public:
    drv8833(gpio_num_t _a_in1, gpio_num_t _a_in2, gpio_num_t _b_in1, gpio_num_t _b_in2, ledc_timer_t _timer)
        : a_in1(_a_in1), a_in2(_a_in2), b_in1(_b_in1), b_in2(_b_in2), timer(_timer) {}

    esp_err_t init(bool skip_ledc);
    esp_err_t spin(uint8_t channel, bool reverse, bool fast_decay, uint8_t duty_cycle) const;

private:
    bool no_ledc = true;
    gpio_num_t a_in1, a_in2, b_in1, b_in2;
    ledc_timer_t timer;

    static constexpr char TAG[] = "drv8833_drv";
    static constexpr uint32_t PWM_FREQ_HZ = 10000;
};
