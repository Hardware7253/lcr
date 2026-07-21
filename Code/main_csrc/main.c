#include <stdint.h>
#include <stdbool.h>

#include "stm32g4xx.h"
#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_conf.h"

#include "system.h"
#include "software_timer.h"
#include "blocking_delay.h"

#include "dut_measure.h"
#include "lcr_math.h"

#include "ui.h"
#include "buttons.h"

#define BUTTON_POLL_HZ 200
#define UI_UPDATE_HZ 10
#define HZ_TO_MS(hz) (1000U / (hz))

static uint32_t button_poll_last_run_ms = 0;
static uint32_t ui_update_last_run_ms = 0;

int main(void) {
    HAL_Init();
    init_clocks();
    init_blocking_delay();
    init_buttons();
    init_ui();

    while (true) {

        // Poll buttons
        if (HAL_GetTick() - button_poll_last_run_ms >= HZ_TO_MS(BUTTON_POLL_HZ)) {
            poll_buttons();
            button_poll_last_run_ms = HAL_GetTick();
            
        }

        // Update UI
        if (HAL_GetTick() - ui_update_last_run_ms >= HZ_TO_MS(UI_UPDATE_HZ)) {
            run_ui();
            ui_update_last_run_ms = HAL_GetTick();
        }
    }

    return 0;
}