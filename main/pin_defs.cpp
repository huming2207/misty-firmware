#include <esp_log.h>
#include "pin_defs.hpp"

#include "hal/gpio_ll.h"


ESP_EVENT_DEFINE_BASE(MISTY_IO_EVENTS);

#define TAG "misty_io"

esp_err_t misty::setup_input_interrupts()
{
    gpio_config_t config = {
        .pin_bit_mask = (1ULL << N_CHG_DONE_PIN) | (1ULL << N_CHARGING_PIN) | (1ULL << PUMP_TRIG_BTN_PIN) | (1ULL << CONFIG_BTN_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };

    gpio_set_pull_mode(PUMP_FAULT_PIN, GPIO_PULLUP_ONLY); // Software workaround for missing the nFAULT's pullup resistor

    auto ret = gpio_config(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO interrupt config failed");
        return ret;
    }

    esp_event_loop_create_default();
    gpio_install_isr_service(0);
    gpio_isr_handler_add(N_CHG_DONE_PIN, chg_done_handler, nullptr);
    gpio_isr_handler_add(N_CHARGING_PIN, charging_handler, nullptr);
    gpio_isr_handler_add(CONFIG_BTN_PIN, config_btn_handler, nullptr);
    gpio_isr_handler_add(PUMP_TRIG_BTN_PIN, pump_trig_btn_handler, nullptr);

    return ESP_OK;
}

static void IRAM_ATTR misty::charging_handler(void* _ctx)
{
    esp_event_isr_post(MISTY_IO_EVENTS, gpio_ll_get_level(&GPIO, N_CHARGING_PIN) == 0 ? CHARGING_ACTIVE : CHARGING_INACTIVE, nullptr, 0, nullptr);
}

void IRAM_ATTR misty::chg_done_handler(void* _ctx)
{
    esp_event_isr_post(MISTY_IO_EVENTS, gpio_ll_get_level(&GPIO, N_CHG_DONE_PIN) == 0 ? CHG_DONE_ACTIVE : CHG_DONE_INACTIVE, nullptr, 0, nullptr);
}

void IRAM_ATTR misty::config_btn_handler(void* _ctx)
{
    if (gpio_ll_get_level(&GPIO, CONFIG_BTN_PIN) == 0) {
        esp_event_isr_post(MISTY_IO_EVENTS, CONFIG_BUTTON_PRESSED, nullptr, 0, nullptr);
    }
}

void IRAM_ATTR misty::pump_trig_btn_handler(void* _ctx)
{
    if (gpio_ll_get_level(&GPIO, PUMP_TRIG_BTN_PIN) == 0) {
        esp_event_isr_post(MISTY_IO_EVENTS, PUMP_TRIG_BUTTON_PRESSED, nullptr, 0, nullptr);
    }
}
