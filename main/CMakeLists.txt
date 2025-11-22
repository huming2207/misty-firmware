idf_component_register(
        SRCS
            "misty_main.cpp" "cron_manager.cpp" "config_server.cpp"
            "driver/hdc2080.cpp" "driver/drv8833.cpp"

        PRIV_REQUIRES
            spi_flash esp_driver_i2c esp_driver_gpio
            esp_driver_ledc hal esp_timer nvs_flash esp_schedule
            esp_http_server
        INCLUDE_DIRS "." "./driver"
)
