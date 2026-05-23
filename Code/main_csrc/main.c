#include <stdint.h>
#include <stdbool.h>

#include "stm32g4xx.h"
#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_conf.h"

#include "system.h"
#include "software_timer.h"
#include "blocking_delay.h"

// #include "dut_measure.h"
// #include "lcr_math.h"

int main(void) {

    HAL_Init();
    init_clocks();
    init_blocking_delay();

    // init_dut_measurement();
   
    // 2 Hz software timer
    // software_timer_t stimer = construct_stimer_f((uint16_t)get_tick_frequency(), 2, HAL_GetTick(), PERIODIC_ST);

    // This would be a 2 second period software timer
    // software_timer_t stimer = construct_stimer_p((uint16_t)get_tick_frequency(), 2000, HAL_GetTick(), PERIODIC_ST);

    // polar_t dut_z = {0};
    // volatile passive_component_t dut = {0};
    // volatile unit_float_t reactive_component_unit_val = {0};

    // start_dut_measurement(TF_10KHZ);

    while (true) {


        // if (is_stimer_finished(&stimer, HAL_GetTick())) {

        // if (get_dut_measurement(&dut_z, 130)) {
        //     dut = calc_passive_component(&dut_z, 10000);
        //     reactive_component_unit_val = convert_to_unit(dut.reactive_component_val);
        //     if (reactive_component_unit_val.val > 1.0F) {
        //         (void) 0;
        //     }
        // }
        
    }

    return 0;
}