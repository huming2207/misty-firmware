#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <esp_schedule.h>

#include "nvs_flash.h"
#include "nvs.h"
#include "nvs_handle.hpp"

#include "esp_bit_defs.h"
#include "pin_defs.hpp"

class sched_manager
{
public:
    static sched_manager &instance()
    {
        static sched_manager _instance;
        return _instance;
    }

    sched_manager(sched_manager const &) = delete;
    void operator=(sched_manager const &) = delete;

    enum pump_bits : uint8_t
    {
        PUMP_0 = BIT(0),
        PUMP_1 = BIT(1),
        PUMP_ALL = PUMP_0 | PUMP_1,
    };

    enum duration_profile : uint8_t
    {
        PROFILE_DRY = 0,
        PROFILE_MODERATE = 1,
        PROFILE_WET = 2,
        PROFILE_COUNT,
    };

    struct __attribute__((packed)) cron_store_entry
    {
        pump_bits select_pumps;
        uint8_t day_of_week;
        union
        {
            struct
            {
                uint8_t hour;
                uint8_t minute;
            } dow;
            int16_t offset_minute;
        };
        uint32_t duration_ms[PROFILE_COUNT];
        esp_schedule_type_t schedule_type;
    };

    struct cron_task_item
    {
        esp_schedule_handle_t scheduler;
        cron_store_entry sched_info;
        char name[NVS_KEY_NAME_MAX_SIZE];
    };

    esp_err_t init();
    esp_err_t load_schedules();
    esp_err_t set_schedule(const char *name, const cron_store_entry *entry);
    esp_err_t get_schedule(const char *name, cron_store_entry *entry_out) const;
    esp_err_t list_all_schedule_names_to_json(char *name_out, size_t len) const;
    esp_err_t delete_schedule(const char *name) const;

private:
    sched_manager() = default;
    void schedule_dispatcher(size_t idx);
    static void schedule_dispatch_task(void *_ctx);
    static void schedule_trigger_callback(esp_schedule_handle_t handle, void *ctx);

    nvs_handle_t nvs = 0;
    QueueHandle_t dispatch_queue = nullptr;

    // Because I'm targeting ESP32-C6 so better off use array instead of vector/deque to save heap
    std::array<cron_task_item, 10> task_items = {};


    static const constexpr char TAG[] = "cronman";
};
