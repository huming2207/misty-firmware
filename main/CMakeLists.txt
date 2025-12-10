idf_component_register(
        SRCS
            "mjson.c"
            "air_sensor.cpp"
            "misty_main.cpp" "sched_manager.cpp" "config_server.cpp"
            "net_configurator.cpp"
            "driver/hdc2080.cpp" "driver/drv8833.cpp"
        PRIV_REQUIRES
            spi_flash esp_driver_i2c esp_driver_gpio
            esp_driver_ledc hal esp_timer nvs_flash esp_schedule
            esp_http_server esp_wifi esp_app_format
        INCLUDE_DIRS "." "./driver"
)
