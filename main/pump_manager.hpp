#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <esp_err.h>
#include <esp_event.h>
#include <bdc_motor.h>


ESP_EVENT_DECLARE_BASE(MISTY_PUMP_EVENTS);

class pump_manager
{
public:
    static pump_manager &instance()
    {
        static pump_manager _instance;
        return _instance;
    }

    void operator=(const pump_manager &) = delete;

    enum pump_events : uint32_t
    {
        PUMP_A_OFF_TIMER_TRIGGERED = 0,
        PUMP_B_OFF_TIMER_TRIGGERED,
        PUMP_FAULT_TRIGGERED,
        PUMP_TRIG_BUTTON_PRESSED,
    };

private:
    pump_manager() = default;

public:
    esp_err_t init();
    esp_err_t run_a(uint32_t duration_ms) const;
    esp_err_t run_b(uint32_t duration_ms) const;

private:
    bool motor_trig_enabled = false;
    bdc_motor_handle_t motor_a = nullptr;
    bdc_motor_handle_t motor_b = nullptr;
    TimerHandle_t motor_a_off_timer = nullptr;
    TimerHandle_t motor_b_off_timer = nullptr;
    static void motor_a_off_timer_cb(TimerHandle_t timer);
    static void motor_b_off_timer_cb(TimerHandle_t timer);
    static void pump_event_handler(void *_ctx, esp_event_base_t evt_base, int32_t evt_id, void *evt_data);
    static constexpr char TAG[] = "pump";
};

