#include "driver/gpio.h"
#include "hdc2080.hpp"

#include "esp_log.h"

hdc2080::hdc2080(i2c_master_bus_handle_t _bus)
{
    i2c_bus = _bus;
}

esp_err_t hdc2080::init(gpio_num_t drdy, gpio_num_t sda, gpio_num_t scl, i2c_port_t port)
{
    if (i2c_bus == nullptr) {
        gpio_reset_pin(sda);
        gpio_reset_pin(scl);
        i2c_master_bus_config_t bus_cfg = {
            .i2c_port = port,
            .sda_io_num = sda,
            .scl_io_num = scl,
            .clk_source = I2C_CLK_SRC_XTAL,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
#if !SOC_I2C_SUPPORT_SLEEP_RETENTION
                .allow_pd = 1,
#else
                .allow_pd = 1,
#endif

            }
        };

        esp_err_t ret = i2c_new_master_bus(&bus_cfg, &i2c_bus);
        if (ret != ESP_OK || i2c_bus == nullptr) {
            ESP_LOGE(TAG, "Can't init I2C bus");
            return ret;
        }

        ESP_LOGI(TAG, "I2C bus created %p", i2c_bus);
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0b1000000,
        .scl_speed_hz =  400000,
        .scl_wait_us = 0,
        .flags = {
            .disable_ack_check = 1,
        },
    };

    esp_err_t ret = i2c_master_bus_add_device(i2c_bus, &dev_cfg, &i2c_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "init: can't set up I2C device: 0x%x %s", ret, esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

esp_err_t hdc2080::read_humidity(float& rh_out) const
{
    uint8_t low = 0, high = 0;
    esp_err_t ret = read_reg(HUMIDITY_LOW, &low, 1000);
    ret = ret ?: read_reg(HUMIDITY_HIGH, &high, 1000);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read humidity: 0x%x", ret);
        return ret;
    }

    uint16_t val = high << 8 | low;
    rh_out = (float)val * 100.0f / 65536.0f;
    return ESP_OK;
}

esp_err_t hdc2080::read_temperature(float& degc_out) const
{
    uint8_t low = 0, high = 0;
    esp_err_t ret = read_reg(TEMPERATURE_LOW, &low, 1000);
    ret = ret ?: read_reg(TEMPERATURE_HIGH, &high, 1000);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read temperature: 0x%x", ret);
        return ret;
    }

    uint16_t val = high << 8 | low;
    degc_out = (((float)val * 165.0f) / 65536.0f) - (40.5f + 0.08f * (3.3f - 1.8f));
    return ESP_OK;
}

esp_err_t hdc2080::reset() const
{
    esp_err_t ret = write_reg(RESET_DRDY_CONF, 0x80, 3000);
    vTaskDelay(pdMS_TO_TICKS(50)); // Is this too long??
    return ret;
}

esp_err_t hdc2080::set_measure_config(bool trigger, bool temperature_only, resolution humidity_res, resolution temp_res) const
{
    uint8_t val = trigger ? 1 : 0;
    val |= temperature_only ? 0b10 : 0;
    val |= (humidity_res << 4);
    val |= (temp_res << 6);
    return write_reg(MEASURE_CONFIG, val, 1000);
}

esp_err_t hdc2080::write_reg(reg_addr reg, uint8_t data, int timeout_ms) const
{
    const uint8_t tx[2] = {reg, data};
    return i2c_master_transmit(i2c_dev, tx, 2, timeout_ms);
}

esp_err_t hdc2080::read_reg(reg_addr reg, uint8_t* data, int timeout_ms) const

{
    if (data == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t reg_val = reg;
    return i2c_master_transmit_receive(i2c_dev, &reg_val, 1, data, 1, timeout_ms);
}

