#pragma once

#include <cstdint>
#include <driver/i2c_master.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class hdc2080
{
public:
    enum reg_addr : uint8_t {
        TEMPERATURE_LOW = 0x00,
        TEMPERATURE_HIGH = 0x01,
        HUMIDITY_LOW = 0x02,
        HUMIDITY_HIGH = 0x03,
        INTERRUPT_DRDY = 0x04,
        TEMPERATURE_MAX = 0x05,
        HUMIDITY_MAX = 0x06,
        INTERRUPT_ENABLE = 0x07,
        TEMPERATURE_OFFSET_ADJ = 0x08,
        HUMIDITY_OFFSET_ADJ = 0x09,
        TEMP_THR_LOW = 0x0A,
        TEMP_THR_HIGH = 0x0B,
        RH_THR_LOW = 0x0C,
        RH_THR_HIGH = 0x0D,
        RESET_DRDY_CONF = 0x0E,
        MEASURE_CONFIG = 0x0F,
        MFG_ID_LOW = 0xFC,
        MFG_ID_HIGH = 0xFD,
        DEVICE_ID_LOW = 0xFE,
        DEVICE_ID_HIGH = 0xFF
    };

    enum resolution : uint8_t {
        RES_14BIT = 0x00,
        RES_11BIT = 0x01,
        RES_9BIT = 0x02,
    };

    explicit hdc2080(i2c_master_bus_handle_t _bus = nullptr);
    esp_err_t init(gpio_num_t drdy, gpio_num_t sda = GPIO_NUM_NC, gpio_num_t scl = GPIO_NUM_NC, i2c_port_t port = I2C_NUM_0);
    esp_err_t read_humidity(float &rh_out) const;
    esp_err_t read_temperature(float &degc_out) const;
    esp_err_t reset() const;
    esp_err_t set_measure_config(bool trigger, bool temperature_only = false, resolution humidity_res = RES_14BIT, resolution temp_res = RES_14BIT) const;

private:
    esp_err_t write_reg(reg_addr reg, uint8_t data, int timeout_ms) const;
    esp_err_t read_reg(reg_addr reg, uint8_t *data, int timeout_ms) const;

    i2c_master_bus_handle_t i2c_bus = nullptr;
    i2c_master_dev_handle_t i2c_dev = nullptr;

    static constexpr uint8_t DEV_ADDR = 0x40;
    static constexpr char TAG[] = "hdc2080";
};
