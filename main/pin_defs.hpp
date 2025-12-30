#pragma once

#include <esp_attr.h>
#include <driver/gpio.h>

#include "esp_event.h"

ESP_EVENT_DECLARE_BASE(MISTY_IO_EVENTS);

namespace misty
{
    static constexpr gpio_num_t N_CHG_DONE_PIN = GPIO_NUM_2;
    static constexpr gpio_num_t N_CHARGING_PIN = GPIO_NUM_3;
    static constexpr gpio_num_t I2C_SDA_PIN = GPIO_NUM_4;
    static constexpr gpio_num_t I2C_SCL_PIN = GPIO_NUM_5;
    static constexpr gpio_num_t TS_DRDY_PIN = GPIO_NUM_6;
    static constexpr gpio_num_t PUMP_TRIG_BTN_PIN = GPIO_NUM_7;
    static constexpr gpio_num_t CONFIG_BTN_PIN = GPIO_NUM_9;
    static constexpr gpio_num_t STATUS_LED_PIN = GPIO_NUM_14;
    static constexpr gpio_num_t PUMP_SLEEP_PIN = GPIO_NUM_18;
    static constexpr gpio_num_t PUMP_FAULT_PIN = GPIO_NUM_19;
    static constexpr gpio_num_t PUMP_AIN1_PIN = GPIO_NUM_20;
    static constexpr gpio_num_t PUMP_AIN2_PIN = GPIO_NUM_21;
    static constexpr gpio_num_t PUMP_BIN1_PIN = GPIO_NUM_22;
    static constexpr gpio_num_t PUMP_BIN2_PIN = GPIO_NUM_23;

    enum io_events
    {
        CHG_DONE_ACTIVE = 0,
        CHG_DONE_INACTIVE = 1,
        CHARGING_ACTIVE = 2,
        CHARGING_INACTIVE = 3,
        CONFIG_BUTTON_PRESSED,
        PUMP_TRIG_BUTTON_PRESSED,
    };

    esp_err_t setup_input_interrupts();
    static void IRAM_ATTR charging_handler(void *_ctx);
    static void IRAM_ATTR chg_done_handler(void *_ctx);
    static void IRAM_ATTR config_btn_handler(void *_ctx);
    static void IRAM_ATTR pump_trig_btn_handler(void *_ctx);
}
