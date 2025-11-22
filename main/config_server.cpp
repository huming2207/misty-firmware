#include "config_server.hpp"

#include "esp_log.h"

esp_err_t config_server::init()
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size = 8192;
    esp_err_t ret = httpd_start(&httpd, &cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "init: can't start httpd");
        return ret;
    }

    httpd_uri_t list_all_schedules_cfg = {
        .uri = "/api/schedule/list",
        .method = HTTP_GET,
        .handler = list_all_schedule_handler,
        .user_ctx = this,
    };
    ret = httpd_register_uri_handler(httpd, &list_all_schedules_cfg);

    httpd_uri_t add_schedule_cfg = {
        .uri = "/api/schedule",
        .method = HTTP_POST,
        .handler = add_schedule_handler,
        .user_ctx = this,
    };
    ret = ret ?: httpd_register_uri_handler(httpd, &add_schedule_cfg);

    httpd_uri_t remove_schedule_cfg = {
        .uri = "/api/schedule",
        .method = HTTP_DELETE,
        .handler = remove_schedule_handler,
        .user_ctx = this,
    };
    ret = ret ?: httpd_register_uri_handler(httpd, &remove_schedule_cfg);

    httpd_uri_t get_schedule_cfg = {
        .uri = "/api/schedule",
        .method = HTTP_GET,
        .handler = get_schedule_handler,
        .user_ctx = this,
    };
    ret = ret ?: httpd_register_uri_handler(httpd, &get_schedule_cfg);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "init: can't register handlers: 0x%x", ret);
    }

    return ret;
}

esp_err_t config_server::stop()
{
    esp_err_t ret = httpd_stop(httpd);
    httpd = nullptr;
    return ret;
}

esp_err_t config_server::list_all_schedule_handler(httpd_req_t* req)
{
    return ESP_OK;
}

esp_err_t config_server::get_schedule_handler(httpd_req_t* req)
{
    return ESP_OK;
}

esp_err_t config_server::add_schedule_handler(httpd_req_t* req)
{
    return ESP_OK;
}

esp_err_t config_server::remove_schedule_handler(httpd_req_t* req)
{
    return ESP_OK;
}
