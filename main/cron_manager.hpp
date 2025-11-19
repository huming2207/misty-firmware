#pragma once

#include <deque>
#include <esp_schedule.h>

#include "nvs_flash.h"
#include "nvs.h"
#include "nvs_handle.hpp"

#include "esp_bit_defs.h"

class cron_manager
{
public:
    static cron_manager &instance()
    {
        static cron_manager _instance;
        return _instance;
    }

    cron_manager(cron_manager const &) = delete;
    void operator=(cron_manager const &) = delete;

    enum pump_bits : uint8_t
    {
        PUMP_0 = BIT(0),
        PUMP_1 = BIT(1),
        PUMP_ALL = PUMP_0 | PUMP_1,
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
        uint32_t duration_ms;
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

private:
    cron_manager() = default;
    static void schedule_trigger_callback(esp_schedule_handle_t handle, void *ctx);

    nvs_handle_t nvs = 0;
    std::deque<cron_task_item> task_items = {};


    static const constexpr char TAG[] = "cronman";
};
