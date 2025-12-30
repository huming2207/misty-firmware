#include "sched_manager.hpp"

#include "air_sensor.hpp"
#include "esp_log.h"
#include "pump_manager.hpp"

esp_err_t sched_manager::init()
{
    esp_err_t ret = nvs_open("cron", NVS_READWRITE, &nvs);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "init: can't open NVS: 0x%x", ret);
        return ret;
    }

    // We don't use NVS functionality provided by ESP schedule because it can't save additional info
    // Instead we do it on our on, so that we can save whatever we want!
    esp_schedule_init(false, nullptr, nullptr);
    dispatch_queue = xQueueCreate(3, sizeof(uint32_t));
    if (dispatch_queue == nullptr) {
        ESP_LOGE(TAG, "init: can't create dispatch queue");
        return ESP_ERR_NO_MEM;
    }

    if (xTaskCreate(schedule_dispatch_task, "cron_dispatch", 4096, this, tskIDLE_PRIORITY + 3, nullptr) != pdPASS) {
        ESP_LOGE(TAG, "init: can't create dispatch task");
        return ESP_ERR_NO_MEM;
    }

    return load_schedules();
}

esp_err_t sched_manager::load_schedules()
{
    nvs_iterator_t nvs_it = nullptr;
    esp_err_t ret = nvs_entry_find_in_handle(nvs, NVS_TYPE_BLOB, &nvs_it);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "load_sched: schedule entry is empty, skip loading");

        for (size_t idx = 0; idx < task_items.size(); idx += 1) {
            if (task_items[idx].scheduler != nullptr) {
                esp_schedule_disable(task_items[idx].scheduler);
                esp_schedule_delete(task_items[idx].scheduler);
            }

            memset(&task_items[idx], 0, sizeof(cron_task_item));
        }

        xQueueReset(dispatch_queue);
        return ESP_OK;
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "load_sched: Can't open NVS handle: 0x%x", ret);
        return ret;
    }

    for (size_t idx = 0; idx < task_items.size(); idx += 1) {
        if (task_items[idx].scheduler != nullptr) {
            esp_schedule_disable(task_items[idx].scheduler);
            esp_schedule_delete(task_items[idx].scheduler);
        }

        memset(&task_items[idx], 0, sizeof(cron_task_item));
    }

    xQueueReset(dispatch_queue);

    size_t item_idx = 0;
    while (nvs_it != nullptr && item_idx < task_items.size()) {
        nvs_entry_info_t entry = {};
        ret = nvs_entry_info(nvs_it, &entry);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "load_sched: can't load NVS entry: 0x%x", ret);
            return ret;
        }

        cron_store_entry item = {};
        size_t item_size = sizeof(cron_store_entry);
        ret = nvs_get_blob(nvs, entry.key, &item, &item_size);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "load_sched: Can't read item %s, returned 0x%x", entry.key, ret);
            return ret;
        }

        if (item_size < sizeof(item)) {
            ESP_LOGE(TAG, "load_sched: corrupted item with wrong size %u want %u", item_size, sizeof(item));

            ret = nvs_entry_next(&nvs_it);
            if (ret != ESP_OK) {
                break;
            }
        }

        strncpy(task_items[item_idx].name, entry.key, sizeof(cron_task_item::name) - 1);
        task_items[item_idx].name[sizeof(cron_task_item::name) - 1] = '\0';

        memcpy(&task_items[item_idx].sched_info, &item, sizeof(item));

        esp_schedule_config_t sched_cfg = {};
        strncpy(sched_cfg.name, entry.key, sizeof(cron_task_item::name) - 1);
        sched_cfg.name[sizeof(cron_task_item::name) - 1] = '\0';

        sched_cfg.priv_data = (void *)item_idx;
        sched_cfg.validity.end_time = 0;
        sched_cfg.validity.start_time = 0;
        sched_cfg.trigger_cb = schedule_trigger_callback;
        sched_cfg.trigger.day.repeat_days = task_items[item_idx].sched_info.day_of_week;
        sched_cfg.trigger.type = task_items[item_idx].sched_info.schedule_type;
        if (sched_cfg.trigger.type == ESP_SCHEDULE_TYPE_DAYS_OF_WEEK) {
            sched_cfg.trigger.hours = task_items[item_idx].sched_info.dow.hour;
            sched_cfg.trigger.minutes = task_items[item_idx].sched_info.dow.minute;
        } else if (sched_cfg.trigger.type == ESP_SCHEDULE_TYPE_SUNRISE || sched_cfg.trigger.type == ESP_SCHEDULE_TYPE_SUNSET) {
            sched_cfg.trigger.solar.offset_minutes = task_items[item_idx].sched_info.offset_minute;
        } else {
            ESP_LOGE(TAG, "Unsupported schedule type %u", sched_cfg.trigger.type);

            ret = nvs_entry_next(&nvs_it);
            if (ret != ESP_OK) {
                break;
            }

            continue;
        }

        task_items[item_idx].scheduler = esp_schedule_create(&sched_cfg);
        if (!task_items[item_idx].scheduler) {
            ESP_LOGE(TAG, "load: can't create scheduler");
            return ESP_ERR_NO_MEM; // Maybe reboot instead??
        }
        esp_schedule_enable(task_items[item_idx].scheduler);
        ESP_LOGI(TAG, "load: inserted item %s, type=%u", task_items[item_idx].name, task_items[item_idx].sched_info.schedule_type);

        ret = nvs_entry_next(&nvs_it);
        item_idx += 1;
        if (ret != ESP_OK || nvs_it == nullptr || item_idx >= task_items.size()) {
            break;
        }
    }

    ESP_LOGI(TAG, "load_sched: done, got %u schedules", item_idx);
    return ESP_OK;
}

esp_err_t sched_manager::set_schedule(const char* name, const cron_store_entry* entry)
{
    ESP_LOGI(TAG, "set: begin");
    nvs_type_t nvs_type = NVS_TYPE_ANY;
    esp_err_t ret = nvs_find_key(nvs, name, &nvs_type);
    if (ret == ESP_ERR_NVS_NOT_FOUND || ret == ESP_ERR_NOT_FOUND) {
        ESP_LOGI(TAG, "set: item %s inserting", name);
        ret = nvs_set_blob(nvs, name, entry, sizeof(cron_store_entry));
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "set: failed to insert: 0x%x", ret);
            return ret;
        }


        ESP_LOGI(TAG, "set: OK, reloading");
        return load_schedules();
    }

    if (ret == ESP_OK) {
        ESP_LOGW(TAG, "set: item %s already exist", name);
        return ESP_ERR_INVALID_STATE;
    } else {
        ESP_LOGE(TAG, "set: NVS error 0x%x", ret);
    }

    return ret;
}

esp_err_t sched_manager::get_schedule(const char* name, cron_store_entry* entry_out) const
{
    size_t len = sizeof(cron_store_entry);
    esp_err_t ret = nvs_get_blob(nvs, name, entry_out, &len);
    return ret;
}

esp_err_t sched_manager::list_all_schedule_names_to_json(char* name_out, size_t len) const
{
    if (const size_t expected_len = (NVS_KEY_NAME_MAX_SIZE + 3) * task_items.size() + 1; name_out == nullptr || len < expected_len) {
        ESP_LOGE(TAG, "Name list buffer too short, name %p, len %u vs %u", name_out, len, expected_len);
        return ESP_ERR_NO_MEM;
    }

    nvs_iterator_t nvs_it = nullptr;
    esp_err_t ret = nvs_entry_find_in_handle(nvs, NVS_TYPE_BLOB, &nvs_it);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "load_sched: Can't open NVS handle: 0x%x", ret);
        return ret;
    }

    size_t out_idx = 0;

    char *out = name_out;
    out[out_idx++] = '[';

    size_t item_idx = 0;
    while (nvs_it != nullptr && item_idx < task_items.size() && out_idx + 2 < len) {
        out[out_idx++] = '"';
        nvs_entry_info_t entry = {};
        ret = nvs_entry_info(nvs_it, &entry);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "load_sched: can't load NVS entry: 0x%x", ret);
            return ret;
        }

        size_t copy_len = strnlen(entry.key, sizeof(nvs_entry_info_t::key));
        memcpy(out + out_idx, entry.key, copy_len);
        out_idx += copy_len;
        out[out_idx++] = '"';

        ret = nvs_entry_next(&nvs_it);
        if (ret != ESP_OK) {
            break;
        }

        if (nvs_it != nullptr) {
            out[out_idx++] = ',';
        } else {
            break;
        }
    }


    out[out_idx++] = ']';
    out[out_idx++] = '\0';
    ESP_LOGI(TAG, "list_all_name: written %u bytes", out_idx);

    return ESP_OK;
}

esp_err_t sched_manager::delete_schedule(const char* name) const
{
    return nvs_erase_key(nvs, name);
}

void sched_manager::schedule_dispatcher(size_t idx)
{
    auto &sensor = air_sensor::instance();
    bool sensor_has_reading = sensor.has_valid_reading();
    float humidity = sensor.average_humidity();
    duration_profile profile = PROFILE_MODERATE;

    if (!sensor_has_reading) {
        profile = PROFILE_MODERATE;
        ESP_LOGI(TAG, "dispatch: no reading, profile set to MODERATE");
    } else {
        if (humidity <= air_sensor::HUMID_DRY_THRESH) {
            profile = PROFILE_DRY;
            ESP_LOGI(TAG, "dispatch: profile set to DRY");
        } else if (humidity <= air_sensor::HUMID_MODERATE_THRESH && humidity > air_sensor::HUMID_DRY_THRESH) {
            profile = PROFILE_MODERATE;
            ESP_LOGI(TAG, "dispatch: profile set to MODERATE");
        } else {
            profile = PROFILE_WET;
            ESP_LOGI(TAG, "dispatch: profile set to WET");
        }
    }

    uint32_t duration_ms = task_items[idx].sched_info.duration_ms[profile];
    if (duration_ms > 3600*1000) {
        ESP_LOGW(TAG, "Duration is too long, set back to 1 hour");
        duration_ms = 3600*1000;
    }

    if ((task_items[idx].sched_info.select_pumps & 0b01) != 0) {
        pump_manager::instance().run_a(duration_ms);
    }

    if ((task_items[idx].sched_info.select_pumps & 0b10) != 0) {
        pump_manager::instance().run_b(duration_ms);
    }
}

void sched_manager::schedule_dispatch_task(void* _ctx)
{
    auto &mgr = instance();

    while (true) {
        size_t idx = SIZE_MAX;
        if (xQueueReceive(mgr.dispatch_queue, &idx, portMAX_DELAY) != pdTRUE) {
            ESP_LOGW(TAG, "dispatch_task: nothing to receive??");
            vTaskDelay(1);
            return;
        }

        if (idx < 0 || idx > mgr.task_items.size()) {
            ESP_LOGW(TAG, "Invalid index value, skipping");
            return;
        }

        ESP_LOGI(TAG, "dispatch_task: got %u", idx);
        mgr.schedule_dispatcher(idx);
        vTaskDelay(1);
    }
}

void sched_manager::schedule_trigger_callback(esp_schedule_handle_t handle, void* ctx) // ctx is the item!!
{
    auto &mgr = instance();
    auto idx = reinterpret_cast<size_t>(ctx);
    ESP_LOGI(TAG, "trigger: enqueue %p %u", ctx, idx);
    xQueueSend(mgr.dispatch_queue, &idx, portMAX_DELAY);
}
