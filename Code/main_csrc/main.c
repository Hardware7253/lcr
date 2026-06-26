#include <stdint.h>
#include <stdbool.h>

#include "stm32g4xx.h"
#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_conf.h"

#include "system.h"
#include "software_timer.h"
#include "blocking_delay.h"

#include "display.h"
#include "dut_measure.h"
#include "lcr_math.h"



int main(void) {
    HAL_Init();
    init_clocks();
    init_blocking_delay();

    init_dut_measurement();

    // Wait to ensure display is reset by RC circuit before communication
    delay_ms(25);

    u8g2_t display;
    init_display(&display, U8G2_R0);
    u8g2_ClearBuffer(&display);
    u8g2_SetFont(&display, u8g2_font_6x13_tr);
    u8g2_DrawStr(&display, 0, 20, "Hello World");
    u8g2_SendBuffer(&display);
   
    // 2 Hz software timer
    // software_timer_t stimer = construct_stimer_f((uint16_t)get_tick_frequency(), 2, HAL_GetTick(), PERIODIC_ST);

    // This would be a 2 second period software timer
    // software_timer_t stimer = construct_stimer_p((uint16_t)get_tick_frequency(), 2000, HAL_GetTick(), PERIODIC_ST);

    start_dut_measurement(TF_10KHZ);

    polar_t dut_z = {0};
    passive_component_t dut = {0};
    volatile unit_float_t reactive_component_unit_val = {0};


    while (true) {

        // if (is_stimer_finished(&stimer, HAL_GetTick())) {

        if (get_dut_measurement(&dut_z, 100.0)) {
            dut = calc_passive_component(&dut_z, 10000);
            reactive_component_unit_val = convert_to_unit(dut.reactive_component_val);
            if (reactive_component_unit_val.val > 1.0F) {
                (void) 0;
            }
        }
        
    }

    return 0;
}