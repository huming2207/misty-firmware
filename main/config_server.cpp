#include <esp_log.h>
#include <esp_app_desc.h>
#include <esp_wifi.h>
#include <sys/time.h>
#include "config_server.hpp"

#include "mjson.h"
#include "net_configurator.hpp"
#include "sched_manager.hpp"

extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[] asm("_binary_index_html_end");

esp_err_t config_server::init()
{
    if (httpd != nullptr) {
        ESP_LOGW(TAG, "init: stopping old instance");
        httpd_stop(httpd);
    }

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

    httpd_uri_t set_time_cfg = {
        .uri = "/api/time",
        .method = HTTP_POST,
        .handler = set_time_handler,
        .user_ctx = this,
    };
    ret = ret ?: httpd_register_uri_handler(httpd, &set_time_cfg);

    httpd_uri_t index_cfg = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = index_handler,
        .user_ctx = this,
    };
    ret = ret ?: httpd_register_uri_handler(httpd, &index_cfg);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "init: can't register handlers: 0x%x", ret);
    }

    ESP_LOGI(TAG, "init: server started");
    return ret;
}

esp_err_t config_server::stop()
{
    if (httpd != nullptr) {
        esp_err_t ret = httpd_stop(httpd);
        httpd = nullptr;
        return ret;
    }

    return ESP_OK;
}

esp_err_t config_server::get_schedule_handler(httpd_req_t* req)
{
    httpd_resp_set_type(req, "application/json");

    char query[64] = { 0 };
    char out[256] = { 0 };
    if (httpd_req_get_url_query_len(req) > sizeof(query) - 1 || httpd_req_get_url_query_len(req) <= 1) {
        esp_err_t ret = sched_manager::instance().list_all_schedule_names_to_json(out, sizeof(out));
        if (ret == ESP_OK) {
            return httpd_resp_send(req, out, (ssize_t)strnlen(out, sizeof(out)));
        } else if (ret == ESP_ERR_NVS_NOT_FOUND) {
            return httpd_resp_sendstr(req, "[]");
        } else {
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Server error");
        }
    }

    if (httpd_req_get_url_query_str(req, query, sizeof(query) - 1) != ESP_OK) {
        esp_err_t ret = sched_manager::instance().list_all_schedule_names_to_json(out, sizeof(out));
        if (ret == ESP_OK) {
            return httpd_resp_send(req, out, (ssize_t)strnlen(out, sizeof(out)));
        } else if (ret == ESP_ERR_NVS_NOT_FOUND) {
            return httpd_resp_sendstr(req, "[]");
        } else {
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Server error");
        }
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
    char query[256] = { 0 };
    if (httpd_req_get_url_query_len(req) > sizeof(query) - 1 || httpd_req_get_url_query_len(req) <= 1) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid argument");
    }

    if (httpd_req_get_url_query_str(req, query, sizeof(query) - 1) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid argument");
    }

    // Get the type first
    char val[16] = { 0 };
    auto ret = httpd_query_key_value(query, "type", val, sizeof(val) - 1);
    val[sizeof(val) - 1] = '\0';
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "add_sched: failed to parse type");
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Can't parse schedule type");
    }

    ESP_LOGI(TAG, "add_sched: type=%s", val);
    sched_manager::cron_store_entry entry = {};
    if (strncmp("sunrise", val, sizeof(val)) == 0) {
        entry.schedule_type = ESP_SCHEDULE_TYPE_SUNRISE;
    } else if (strncmp("sunset", val, sizeof(val)) == 0) {
        entry.schedule_type = ESP_SCHEDULE_TYPE_SUNSET;
    } else if (strncmp("dow", val, sizeof(val)) == 0) {
        entry.schedule_type = ESP_SCHEDULE_TYPE_DAYS_OF_WEEK;
    } else {
        ESP_LOGW(TAG, "add_sched: invalid type");
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid schedule type");
    }

    // Pump selection
    memset(val, 0, sizeof(val));
    ret = httpd_query_key_value(query, "pump", val, sizeof(val) - 1);
    val[sizeof(val) - 1] = '\0';
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "add_sched: failed to parse pump");
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Can't parse schedule type");
    }

    auto pump_select = strtol(val, nullptr, 10);
    ESP_LOGI(TAG, "add_sched: pump=%s parsed to %ld", val, pump_select);
    if (pump_select < 1 || pump_select > sched_manager::PUMP_ALL) {
        ESP_LOGW(TAG, "add_sched: invalid pump selection");
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid pump selection");
    }

    entry.select_pumps = (sched_manager::pump_bits)(pump_select & 0xff);

    // DoW
    memset(val, 0, sizeof(val));
    ret = httpd_query_key_value(query, "dow", val, sizeof(val) - 1);
    val[sizeof(val) - 1] = '\0';
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "add_sched: failed to parse DoW");
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Can't parse schedule DoW");
    }

    auto dow = strtol(val, nullptr, 10);
    ESP_LOGI(TAG, "add_sched: dow=%s parsed to %ld", val, dow);
    if (dow < 1) {
        ESP_LOGW(TAG, "add_sched: invalid DoW selection");
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid day of week selection");
    }

    entry.day_of_week = (uint8_t)(dow & 0xff);


    // DoW hours and minutes
    if (entry.schedule_type == ESP_SCHEDULE_TYPE_DAYS_OF_WEEK) {
        memset(val, 0, sizeof(val));
        ret = httpd_query_key_value(query, "hour", val, sizeof(val) - 1);
        val[sizeof(val) - 1] = '\0';
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "add_sched: failed to parse DoW hour");
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Can't parse dow.hour type");
        }

        auto hour = strtol(val, nullptr, 10);
        ESP_LOGI(TAG, "add_sched: dow.hour=%s parsed to %ld", val, dow);
        if (hour < 1 || hour >= 24) {
            ESP_LOGW(TAG, "add_sched: invalid DoW hour");
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid DoW hour");
        }

        entry.dow.hour = (uint8_t)(hour & 0xff);

        memset(val, 0, sizeof(val));
        ret = httpd_query_key_value(query, "min", val, sizeof(val) - 1);
        val[sizeof(val) - 1] = '\0';
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "add_sched: failed to parse DoW minute");
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Can't parse dow.minute type");
        }

        auto min = strtol(val, nullptr, 10);
        ESP_LOGI(TAG, "add_sched: dow.min=%s parsed to %ld", val, dow);
        if (min < 1 || min >= 60) {
            ESP_LOGW(TAG, "add_sched: invalid DoW minute");
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid DoW minute");
        }

        entry.dow.minute = (uint8_t)(min & 0xff);
    } else {
        memset(val, 0, sizeof(val));
        ret = httpd_query_key_value(query, "off", val, sizeof(val) - 1);
        val[sizeof(val) - 1] = '\0';
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "add_sched: failed to parse DoW hour");
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Can't parse offset type");
        }

        auto offset = (int16_t)strtol(val, nullptr, 10);
        ESP_LOGI(TAG, "add_sched: sun.offset=%s parsed to %d", val, dow);

        entry.offset_minute = offset;
    }

    memset(val, 0, sizeof(val));
    ret = httpd_query_key_value(query, "durd", val, sizeof(val) - 1);
    val[sizeof(val) - 1] = '\0';
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "add_sched: failed to parse dry duration");
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Can't parse dry duration");
    }

    auto dry_dur = (uint32_t)strtol(val, nullptr, 10);
    ESP_LOGI(TAG, "add_sched: dry duration=%s parsed to %d", val, dow);

    entry.duration_ms[sched_manager::PROFILE_DRY] = dry_dur;

    memset(val, 0, sizeof(val));
    ret = httpd_query_key_value(query, "durm", val, sizeof(val) - 1);
    val[sizeof(val) - 1] = '\0';
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "add_sched: failed to parse moderate duration");
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Can't parse moderate duration");
    }

    auto moderate_dur = (uint32_t)strtol(val, nullptr, 10);
    ESP_LOGI(TAG, "add_sched: moderate duration=%s parsed to %d", val, dow);

    entry.duration_ms[sched_manager::PROFILE_MODERATE] = moderate_dur;

    memset(val, 0, sizeof(val));
    ret = httpd_query_key_value(query, "durw", val, sizeof(val) - 1);
    val[sizeof(val) - 1] = '\0';
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "add_sched: failed to parse wet duration");
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Can't parse wet duration");
    }

    auto wet_dur = (uint32_t)strtol(val, nullptr, 10);
    ESP_LOGI(TAG, "add_sched: wet duration=%s parsed to %d", val, dow);

    entry.duration_ms[sched_manager::PROFILE_MODERATE] = wet_dur;

    memset(val, 0, sizeof(val));
    ret = httpd_query_key_value(query, "name", val, sizeof(val) - 1);
    val[sizeof(val) - 1] = '\0';
    if (strnlen(val, sizeof(val)) > NVS_KEY_NAME_MAX_SIZE || strnlen(val, sizeof(val)) == 0) {
        ESP_LOGE(TAG, "add_sched: Invalid name");
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid name");
    }

    ret = sched_manager::instance().set_schedule(val, &entry);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "add_sched: set schedule failed: 0x%x", ret);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Set schedule failed");
    }

    ret = httpd_resp_set_status(req, "202 Accepted");
    ret = ret ?: httpd_resp_sendstr(req, "OK");
    return ret;
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

esp_err_t config_server::set_time_handler(httpd_req_t* req)
{
    char buf[128] = { 0 };
    int ret = 0, remaining = req->content_len;

    if (remaining >= sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Content too long");
        return ESP_FAIL;
    }

    if (remaining > 0) {
        ret = httpd_req_recv(req, buf, remaining);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                httpd_resp_send_408(req);
            }
            return ESP_FAIL;
        }
        buf[ret] = '\0';
    }

    double now_ts = 0;
    if (mjson_get_number(buf, ret, "$.now", &now_ts) == 0) {
        ESP_LOGE(TAG, "set_time: failed to parse JSON: %s", buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_OK;
    }

    struct timeval tv;
    tv.tv_sec = (time_t)now_ts;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);

    ESP_LOGI(TAG, "Time updated to %lld", (long long)tv.tv_sec);

    httpd_resp_set_status(req, "202 Accepted");
    return httpd_resp_sendstr(req, "OK");
}

esp_err_t config_server::index_handler(httpd_req_t* req)
{
    httpd_resp_set_type(req, "text/html");
    const size_t len = index_html_end - index_html_start;
    return httpd_resp_send(req, index_html_start, (ssize_t)len);
}
