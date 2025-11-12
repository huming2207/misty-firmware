idf_component_register(SRCS "misty_main.cpp" "driver/hdc2080.cpp"
                       PRIV_REQUIRES spi_flash esp_driver_i2c
                       INCLUDE_DIRS "." "./driver")
