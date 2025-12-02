#pragma once

#include <esp_http_server.h>

#include "nvs.h"

class config_server
{
public:
    esp_err_t init();
    esp_err_t stop();

private:
    static void noop_free(void *_ptr) {} // Bypass the auto free() for the context pointer
    static esp_err_t get_schedule_handler(httpd_req_t *req);
    static esp_err_t add_schedule_handler(httpd_req_t *req);
    static esp_err_t remove_schedule_handler(httpd_req_t *req);
    static esp_err_t set_wifi_config_handler(httpd_req_t *req);
    static esp_err_t get_wifi_config_handler(httpd_req_t *req);
    static esp_err_t get_firmware_info_handler(httpd_req_t *req);
    httpd_handle_t httpd = nullptr;
    nvs_handle_t nvs = 0; // Not to be confused with scheduler's NVS - this is for misc configs (e.g. WiFi)

private:
    static constexpr char TAG[] = "cfg_server";
};
