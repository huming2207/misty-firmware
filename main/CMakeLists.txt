idf_component_register(
        SRCS
            "misty_main.cpp"
            "driver/hdc2080.cpp" "driver/drv8833.cpp"
            "external/ccronexpr.c"
        PRIV_REQUIRES spi_flash esp_driver_i2c esp_driver_gpio esp_driver_ledc hal
        INCLUDE_DIRS "." "./driver" "./external"
)
