#pragma once

#include <cstdint>
#include <driver/i2c_master.h>

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

private:


    static constexpr uint8_t DEV_ADDR = 0x40;
};
