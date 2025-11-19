#include "cron_manager.hpp"

#include "esp_log.h"

esp_err_t cron_manager::init()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ret = nvs_open("cron", NVS_READWRITE, &nvs);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "init: can't open NVS: 0x%x", ret);
        return ret;
    }

    // We don't use NVS functionality provided by ESP schedule because it can't save additional info
    // Instead we do it on our on, so that we can save whatever we want!
    esp_schedule_init(false, nullptr, nullptr);

    return load_schedules();
}

esp_err_t cron_manager::load_schedules()
{
    nvs_iterator_t nvs_it = nullptr;
    esp_err_t ret = nvs_entry_find_in_handle(nvs, NVS_TYPE_BLOB, &nvs_it);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "load_sched: Can't open NVS handle: 0x%x", ret);
        return ret;
    }

    for (auto &item : task_items) {
        if (item.scheduler != nullptr) {
            esp_schedule_delete(item.scheduler);
        }
    }

    task_items.clear();

    while (nvs_it != nullptr) {
        nvs_entry_info_t entry = {};
        ret = nvs_entry_info(nvs_it, &entry);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "load_sched: can't load NVS entry: 0x%x", ret);
            return ret;
        }

        cron_store_entry item = {};
        size_t item_size = 0;
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

        cron_task_item task_item = {};
        strncpy(task_item.name, entry.key, sizeof(cron_task_item::name) - 1);
        task_item.name[sizeof(cron_task_item::name) - 1] = '\0';

        memcpy(&task_item.sched_info, &item, sizeof(item));

        auto *task_item_ptr = &task_items.emplace_back(task_item);
        esp_schedule_config_t sched_cfg = {};
        strncpy(sched_cfg.name, entry.key, sizeof(cron_task_item::name) - 1);
        sched_cfg.name[sizeof(cron_task_item::name) - 1] = '\0';

        sched_cfg.priv_data = task_item_ptr;
        sched_cfg.validity.end_time = 0;
        sched_cfg.validity.start_time = 0;
        sched_cfg.trigger_cb = schedule_trigger_callback;
        sched_cfg.trigger.day.repeat_days = task_item.sched_info.day_of_week;
        sched_cfg.trigger.type = task_item.sched_info.schedule_type;
        if (sched_cfg.trigger.type == ESP_SCHEDULE_TYPE_DAYS_OF_WEEK) {
            sched_cfg.trigger.hours = task_item.sched_info.dow.hour;
            sched_cfg.trigger.minutes = task_item.sched_info.dow.minute;
        } else if (sched_cfg.trigger.type == ESP_SCHEDULE_TYPE_SUNRISE || sched_cfg.trigger.type == ESP_SCHEDULE_TYPE_SUNSET) {
            sched_cfg.trigger.solar.offset_minutes = task_item.sched_info.offset_minute;
        } else {
            ESP_LOGE(TAG, "Unsupported schedule type %u", sched_cfg.trigger.type);

            ret = nvs_entry_next(&nvs_it);
            if (ret != ESP_OK) {
                break;
            }

            continue;
        }

        task_item_ptr->scheduler = esp_schedule_create(&sched_cfg);
        if (!task_item_ptr->scheduler) {
            ESP_LOGE(TAG, "load: can't create scheduler");
            return ESP_ERR_NO_MEM; // Maybe reboot instead??
        }
        esp_schedule_enable(task_item_ptr->scheduler);
        ESP_LOGI(TAG, "load: inserted item %s, type=%u", task_item_ptr->name, task_item_ptr->sched_info.schedule_type);

        ret = nvs_entry_next(&nvs_it);
        if (ret != ESP_OK) {
            break;
        }
    }

    return ESP_OK;
}

void cron_manager::schedule_trigger_callback(esp_schedule_handle_t handle, void* ctx) // ctx is the item!!
{

}
