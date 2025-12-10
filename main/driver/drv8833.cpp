#include "esp_log.h"
#include <hal/gpio_ll.h>
#include "drv8833.hpp"


esp_err_t drv8833::init()
{
    gpio_config_t config = {
        .pin_bit_mask = (1ULL << a_in1) | (1ULL << a_in2) | (1ULL << b_in1) | (1ULL << b_in2),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t ret = gpio_config(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Can't setup GPIO");
        return ret;
    }

    no_ledc = timer != LEDC_TIMER_MAX;
    if (no_ledc) {
        return ESP_OK;
    }

    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = timer,
        .freq_hz = PWM_FREQ_HZ,
        .clk_cfg = LEDC_USE_RC_FAST_CLK,
        .deconfigure = false,
    };

    ret = ledc_timer_config(&ledc_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "init: Can't set timer config");
        return ret;
    }

    ledc_channel_config_t ledc_chan_a1 = {
        .gpio_num = a_in1,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = timer,
        .duty = 0,
        .hpoint = 0, // Horizontal point, set 0 since no need for phrase shifting,
        .sleep_mode = LEDC_SLEEP_MODE_KEEP_ALIVE, // Tickless RTOS probably need this??
        .flags = {
            .output_invert = 0,
        }
    };

    ret = ledc_channel_config(&ledc_chan_a1);

    ledc_channel_config_t ledc_chan_a2 = {
        .gpio_num = a_in2,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_1,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = timer,
        .duty = 0,
        .hpoint = 0, // Horizontal point, set 0 since no need for phrase shifting,
        .sleep_mode = LEDC_SLEEP_MODE_KEEP_ALIVE, // Tickless RTOS probably need this??
        .flags = {
            .output_invert = 0,
        }
    };

    ret = ret ?: ledc_channel_config(&ledc_chan_a2);

    ledc_channel_config_t ledc_chan_b1 = {
        .gpio_num = b_in1,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_2,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = timer,
        .duty = 0,
        .hpoint = 0, // Horizontal point, set 0 since no need for phrase shifting,
        .sleep_mode = LEDC_SLEEP_MODE_KEEP_ALIVE, // Tickless RTOS probably need this??
        .flags = {
            .output_invert = 0,
        }
    };

    ret = ret ?: ledc_channel_config(&ledc_chan_b1);

    ledc_channel_config_t ledc_chan_b2 = {
        .gpio_num = b_in2,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_3,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = timer,
        .duty = 0,
        .hpoint = 0, // Horizontal point, set 0 since no need for phrase shifting,
        .sleep_mode = LEDC_SLEEP_MODE_KEEP_ALIVE, // Tickless RTOS probably need this??
        .flags = {
            .output_invert = 0,
        }
    };

    ret = ret ?: ledc_channel_config(&ledc_chan_b2);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "init: Can't set LEDC channel");
        return ret;
    }

    return ret;
}

void drv8833::set_sleep(bool enable_sleep) const
{
    gpio_ll_set_level(&GPIO, sleep, enable_sleep ? 0 : 1);
}

esp_err_t drv8833::spin(uint8_t channel, bool reverse, bool fast_decay, uint8_t duty_cycle) const
{
    if (no_ledc) {
        if (channel == 0) {
            if (!reverse) {
                gpio_ll_set_level(&GPIO, a_in1, duty_cycle == 0 ? 0 : 1);
                gpio_ll_set_level(&GPIO, a_in2, 0);
            } else {
                gpio_ll_set_level(&GPIO, a_in2, duty_cycle == 0 ? 0 : 1);
                gpio_ll_set_level(&GPIO, a_in1, 0);
            }
        } else if (channel == 1) {
            if (!reverse) {
                gpio_ll_set_level(&GPIO, b_in1, duty_cycle == 0 ? 0 : 1);
                gpio_ll_set_level(&GPIO, b_in2, 0);
            } else {
                gpio_ll_set_level(&GPIO, b_in2, duty_cycle == 0 ? 0 : 1);
                gpio_ll_set_level(&GPIO, b_in1, 0);
            }
        } else {
            ESP_LOGE(TAG, "Invalid channel: %u", channel);
            return ESP_ERR_INVALID_ARG;
        }
    } else {
        esp_err_t ret = ESP_OK;
        if (channel == 0) {
            if (!reverse) {
                ret = ret ?: ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty_cycle, 0);
                ret = ret ?: ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, fast_decay ? 0 : 255, 0);
            } else {
                ret = ret ?: ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty_cycle, 0);
                ret = ret ?: ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, fast_decay ? 0 : 255, 0);
            }
        } else if (channel == 1) {
            if (!reverse) {
                ret = ret ?: ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, duty_cycle, 0);
                ret = ret ?: ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_3, fast_decay ? 0 : 255, 0);
            } else {
                ret = ret ?: ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_3, duty_cycle, 0);
                ret = ret ?: ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, fast_decay ? 0 : 255, 0);
            }
        } else {
            ESP_LOGE(TAG, "Invalid channel: %u", channel);
            return ESP_ERR_INVALID_ARG;
        }

        return ret;
    }


    return ESP_OK;
}
