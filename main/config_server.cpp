#include <esp_log.h>
#include <esp_app_desc.h>
#include <esp_wifi.h>
#include "config_server.hpp"

#include "mjson.h"
#include "net_configurator.hpp"
#include "sched_manager.hpp"

esp_err_t config_server::init()
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size = 8192;
    esp_err_t ret = httpd_start(&httpd, &cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "init: can't start httpd");
        return ret;
    }

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

    httpd_uri_t set_wifi_handler = {
        .uri = "/api/wifi",
        .method = HTTP_POST,
        .handler = set_wifi_config_handler,
        .user_ctx = this,
    };
    ret = ret ?: httpd_register_uri_handler(httpd, &set_wifi_handler);

    httpd_uri_t get_fw_handler = {
        .uri = "/api/fwinfo",
        .method = HTTP_GET,
        .handler = get_firmware_info_handler,
        .user_ctx = this,
    };
    ret = ret ?: httpd_register_uri_handler(httpd, &get_fw_handler);

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

esp_err_t config_server::get_schedule_handler(httpd_req_t* req)
{
    httpd_resp_set_type(req, "application/json");

    char query[64] = { 0 };
    char out[152] = { 0 };
    if (httpd_req_get_url_query_len(req) > sizeof(query) - 1 || httpd_req_get_url_query_len(req) <= 1) {
        sched_manager::instance().list_all_schedule_names_to_json(out, sizeof(out));
        return httpd_resp_send(req, out, (ssize_t)strnlen(out, sizeof(out)));
    }

    if (httpd_req_get_url_query_str(req, query, sizeof(query) - 1) != ESP_OK) {
        sched_manager::instance().list_all_schedule_names_to_json(out, sizeof(out));
        return httpd_resp_send(req, out, (ssize_t)strnlen(out, sizeof(out)));
    }

    char name[16] = { 0 };
    auto ret = httpd_query_key_value(query, "name", name, sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';

    sched_manager::cron_store_entry entry = {};
    ret = ret ?: sched_manager::instance().get_schedule(name, &entry);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get schedule: 0x%x", ret);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to get schedule");
    }

    int len = 0;
    if (entry.schedule_type == ESP_SCHEDULE_TYPE_DAYS_OF_WEEK) {
        len = snprintf(out, sizeof(out), R"({"name":"%s","pump":%u,"dow":%u,"h":%u,"m":%u,"duration":[%lu,%lu,%lu],"type":%u})",
            name, entry.select_pumps, entry.day_of_week, entry.dow.hour, entry.dow.minute,
            entry.duration_ms[sched_manager::PROFILE_DRY], entry.duration_ms[sched_manager::PROFILE_MODERATE],
            entry.duration_ms[sched_manager::PROFILE_WET], (uint8_t)entry.schedule_type);
    } else if (entry.schedule_type == ESP_SCHEDULE_TYPE_SUNRISE || entry.schedule_type == ESP_SCHEDULE_TYPE_SUNSET) {
        len = snprintf(out, sizeof(out), R"({"name":"%s","pump":%u,"dow":%u,"offset":%d,"duration":[%lu,%lu,%lu],"type":%u})",
           name, entry.select_pumps, entry.day_of_week, entry.offset_minute,
           entry.duration_ms[sched_manager::PROFILE_DRY], entry.duration_ms[sched_manager::PROFILE_MODERATE],
           entry.duration_ms[sched_manager::PROFILE_WET], (uint8_t)entry.schedule_type);
    } else {
        ESP_LOGE(TAG, "Invalid schedule type %u (probably corrupted?)", entry.schedule_type);
        return httpd_resp_send_err(req, HTTPD_505_VERSION_NOT_SUPPORTED, "Invalid schedule type");
    }

    if (len < 1) {
        ESP_LOGE(TAG, "Can't format output JSON");
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Can't format output JSON");
    }

    return httpd_resp_send(req, out, (ssize_t)strnlen(out, sizeof(out)));
}

esp_err_t config_server::add_schedule_handler(httpd_req_t* req)
{
    httpd_resp_set_type(req, "application/json");
    auto *ctx = (config_server *)req->user_ctx;
    if (ctx != nullptr) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid user context");
    }



    return ESP_OK;
}

esp_err_t config_server::remove_schedule_handler(httpd_req_t* req)
{
    char query[64] = { 0 };
    if (httpd_req_get_url_query_len(req) > sizeof(query) - 1 || httpd_req_get_url_query_len(req) <= 1) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid argument");
    }

    if (httpd_req_get_url_query_str(req, query, sizeof(query) - 1) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid argument");
    }

    char name[16] = { 0 };
    auto ret = httpd_query_key_value(query, "name", name, sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';

    ret = ret ?: sched_manager::instance().delete_schedule(name);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to delete %s", name);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to delete schedule");
    }

    ret = httpd_resp_set_status(req, "202 Accepted");
    ret = ret ?: httpd_resp_sendstr(req, "OK");
    return ret;
}

esp_err_t config_server::set_wifi_config_handler(httpd_req_t* req)
{
    char query[96] = { 0 };
    if (httpd_req_get_url_query_len(req) > sizeof(query) - 1 || httpd_req_get_url_query_len(req) <= 1) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid argument length");
    }

    if (httpd_req_get_url_query_str(req, query, sizeof(query) - 1) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid argument");
    }

    wifi_config_t config = {};
    auto ret = httpd_query_key_value(query, "ssid", (char *)config.sta.ssid, sizeof(wifi_config_t::sta.ssid) - 1);
    config.sta.ssid[sizeof(wifi_config_t::sta.ssid) - 1] = '\0';

    ret = ret ?: httpd_query_key_value(query, "pwd", (char *)config.sta.password, sizeof(wifi_config_t::sta.password) - 1);
    config.sta.password[sizeof(wifi_config_t::sta.password) - 1] = '\0';

    ret = ret ?: net_configurator::instance().set_wifi_config(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set wifi: 0x%x", ret);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to set WiFi");
    }

    httpd_resp_set_status(req, "202 Accepted");
    return httpd_resp_sendstr(req, "OK");
}

esp_err_t config_server::get_wifi_config_handler(httpd_req_t* req)
{
    httpd_resp_set_type(req, "application/json");
    auto *ctx = (config_server *)req->user_ctx;
    if (ctx != nullptr) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid user context");
    }
    return ESP_OK;
}

esp_err_t config_server::get_firmware_info_handler(httpd_req_t* req)
{
    httpd_resp_set_type(req, "application/json");

    char out[192] = { 0 };
    int len = snprintf(out, sizeof(out), R"({"sdk":"%s","fw":"%s","compDate":"%s"})", IDF_VER, esp_app_get_description()->version, esp_app_get_description()->date);
    if (len < 2) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid firmware info");
    }

    return httpd_resp_send(req, out, len);
}
