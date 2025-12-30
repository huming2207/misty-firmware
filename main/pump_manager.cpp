#include <bdc_motor.h>
#include "pump_manager.hpp"

#include "esp_log.h"
#include "pin_defs.hpp"
#include "hal/gpio_ll.h"
#include "soc/gpio_struct.h"

ESP_EVENT_DEFINE_BASE(MISTY_PUMP_EVENTS);


esp_err_t pump_manager::init()
{
    constexpr bdc_motor_config_t motor_a_cfg = {
        .pwma_gpio_num = misty::PUMP_AIN1_PIN,
        .pwmb_gpio_num = misty::PUMP_AIN2_PIN,
        .pwm_freq_hz = 20000,
    };

    constexpr bdc_motor_config_t motor_b_cfg = {
        .pwma_gpio_num = misty::PUMP_BIN1_PIN,
        .pwmb_gpio_num = misty::PUMP_BIN2_PIN,
        .pwm_freq_hz = 20000,
    };

    constexpr bdc_motor_mcpwm_config_t mcpwm_config = {
        .group_id = 0,
        .resolution_hz = 2000000,
    };

    esp_err_t ret = bdc_motor_new_mcpwm_device(&motor_a_cfg, &mcpwm_config, &motor_a);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Can't create motor A: 0x%x", ret);
        return ret;
    }

    ret = bdc_motor_new_mcpwm_device(&motor_b_cfg, &mcpwm_config, &motor_b);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Can't create motor B: 0x%x", ret);
        return ret;
    }

    motor_a_off_timer = xTimerCreate("motor_a_off", 1, pdFALSE, this, motor_a_off_timer_cb);
    if (motor_a_off_timer == nullptr) {
        ESP_LOGE(TAG, "Failed to create motor A off timer");
        return ESP_ERR_NO_MEM;
    }

    motor_b_off_timer = xTimerCreate("motor_b_off", 1, pdFALSE, this, motor_a_off_timer_cb);
    if (motor_b_off_timer == nullptr) {
        ESP_LOGE(TAG, "Failed to create motor B off timer");
        return ESP_ERR_NO_MEM;
    }

    gpio_config_t pump_fault_cfg = {
        .pin_bit_mask = (1ULL << misty::PUMP_FAULT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };

    ret = gpio_config(&pump_fault_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO interrupt config failed");
        return ret;
    }

    gpio_config_t pump_sleep_cfg = {
        .pin_bit_mask = (1ULL << misty::PUMP_SLEEP_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ret = gpio_config(&pump_sleep_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO interrupt config failed");
        return ret;
    }

    esp_event_loop_create_default();
    esp_event_handler_register(MISTY_PUMP_EVENTS, ESP_EVENT_ANY_ID, pump_event_handler, nullptr);
    esp_event_handler_register(MISTY_IO_EVENTS, ESP_EVENT_ANY_ID, pump_event_handler, nullptr);

    return ret;
}

esp_err_t pump_manager::run_a(uint32_t duration_ms)
{
    gpio_ll_set_level(&GPIO, misty::PUMP_SLEEP_PIN, 1);
    motor_a_running = true;

    if (xTimerChangePeriod(motor_a_off_timer, pdMS_TO_TICKS(duration_ms), pdMS_TO_TICKS(10000)) == pdFAIL) {
        ESP_LOGE(TAG, "Can't configure timer A!");
        return ESP_ERR_TIMEOUT;
    }

    if (xTimerStart(motor_a_off_timer, pdMS_TO_TICKS(10000)) == pdFAIL) {
        ESP_LOGE(TAG, "Can't start timer A!");
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = bdc_motor_enable(motor_a);
    ret = ret ?: bdc_motor_forward(motor_a);
    ret = ret ?: bdc_motor_set_speed(motor_a, 100);
    return ret;
}

esp_err_t pump_manager::run_b(uint32_t duration_ms)
{
    gpio_ll_set_level(&GPIO, misty::PUMP_SLEEP_PIN, 1);
    motor_b_running = true;

    if (xTimerChangePeriod(motor_b_off_timer, pdMS_TO_TICKS(duration_ms), pdMS_TO_TICKS(10000)) == pdFAIL) {
        ESP_LOGE(TAG, "Can't configure timer!");
        return ESP_ERR_TIMEOUT;
    }

    if (xTimerStart(motor_b_off_timer, pdMS_TO_TICKS(10000)) == pdFAIL) {
        ESP_LOGE(TAG, "Can't start timer B!");
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = bdc_motor_enable(motor_b);
    ret = ret ?: bdc_motor_forward(motor_b);
    ret = ret ?: bdc_motor_set_speed(motor_b, 100);
    return ret;
}

void pump_manager::motor_a_off_timer_cb(TimerHandle_t timer)
{
    esp_event_post(MISTY_PUMP_EVENTS, PUMP_A_OFF_TIMER_TRIGGERED, nullptr, 0, portMAX_DELAY);
}

void pump_manager::motor_b_off_timer_cb(TimerHandle_t timer)
{
    esp_event_post(MISTY_PUMP_EVENTS, PUMP_B_OFF_TIMER_TRIGGERED, nullptr, 0, portMAX_DELAY);
}

void pump_manager::pump_event_handler(void* _ctx, esp_event_base_t evt_base, int32_t evt_id, void* evt_data)
{
    auto &pump = instance();
    if (evt_base == MISTY_PUMP_EVENTS) {
        switch (evt_id) {
            case PUMP_A_OFF_TIMER_TRIGGERED: {
                ESP_LOGI(TAG, "Pump A stop timer triggered, stopping");
                bdc_motor_brake(pump.motor_a);
                bdc_motor_disable(pump.motor_a);
                pump.motor_a_running = false;

                if (!pump.motor_a_running && !pump.motor_b_running) {
                    gpio_ll_set_level(&GPIO, misty::PUMP_SLEEP_PIN, 0);
                }

                break;
            }

            case PUMP_B_OFF_TIMER_TRIGGERED: {
                ESP_LOGI(TAG, "Pump B stop timer triggered, stopping");
                bdc_motor_brake(pump.motor_b);
                bdc_motor_disable(pump.motor_b);
                pump.motor_b_running = false;

                if (!pump.motor_a_running && !pump.motor_b_running) {
                    gpio_ll_set_level(&GPIO, misty::PUMP_SLEEP_PIN, 0);
                }

                break;
            }

            case PUMP_FAULT_TRIGGERED: {
                ESP_LOGW(TAG, "Pump fault detected!!!");
                bdc_motor_brake(pump.motor_a);
                bdc_motor_disable(pump.motor_a);

                bdc_motor_brake(pump.motor_b);
                bdc_motor_disable(pump.motor_b);
                break;
            }

            default: {
                ESP_LOGW(TAG, "Unhandled event %ld", evt_id);
                break;
            }
        }
    } else if (evt_base == MISTY_IO_EVENTS) {
        if (evt_id == misty::PUMP_TRIG_BUTTON_PRESSED) {
            pump.motor_trig_enabled = !pump.motor_trig_enabled;
            if (pump.motor_trig_enabled) {
                ESP_LOGW(TAG, "Pump test enabled");
                pump.motor_a_running = true;
                pump.motor_b_running = true;
                gpio_ll_set_level(&GPIO, misty::PUMP_SLEEP_PIN, 1);
                bdc_motor_enable(pump.motor_a);
                bdc_motor_forward(pump.motor_a);
                bdc_motor_set_speed(pump.motor_a, 100);

                bdc_motor_enable(pump.motor_b);
                bdc_motor_forward(pump.motor_b);
                bdc_motor_set_speed(pump.motor_b, 100);
            } else {
                ESP_LOGW(TAG, "Pump test disabled");
                bdc_motor_brake(pump.motor_a);
                bdc_motor_disable(pump.motor_a);

                bdc_motor_brake(pump.motor_b);
                bdc_motor_disable(pump.motor_b);

                pump.motor_a_running = false;
                pump.motor_b_running = false;
                gpio_ll_set_level(&GPIO, misty::PUMP_SLEEP_PIN, 0);
            }
        }
    }


}
