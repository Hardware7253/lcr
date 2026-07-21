#include "range_resistor.h"
#include "stm32g4xx_hal.h"

#define PINS_CLK_ENABLE __HAL_RCC_GPIOA_CLK_ENABLE

#define G0_PIN GPIO_PIN_2
#define G0_BUS GPIOA

#define R0_PIN GPIO_PIN_3
#define R0_BUS GPIOA

#define R1_PIN GPIO_PIN_4
#define R1_BUS GPIOA

const range_resistor_t RR_DEFAULT = RR_100K;
const gain_resistor_t GR_DEFAULT = GR_10K;

// Initialise GPIO for switching the range/gain resistors
void init_range_resistors(void) {
    PINS_CLK_ENABLE();

    GPIO_InitTypeDef relay_pin_cfg = {
        .Pin = G0_PIN,
        .Mode = GPIO_MODE_OUTPUT_PP,
        .Pull = GPIO_NOPULL,
        .Speed = GPIO_SPEED_FREQ_LOW,
    };
    HAL_GPIO_Init(G0_BUS, &relay_pin_cfg); 
    relay_pin_cfg.Pin = R0_PIN;
    HAL_GPIO_Init(R0_BUS, &relay_pin_cfg); 
    relay_pin_cfg.Pin = R1_PIN;
    HAL_GPIO_Init(R1_BUS, &relay_pin_cfg); 
}

// Switches is the desired range resistor
// Returns the resistance (in ohms) of the selected range resistor
float set_range_resistor(range_resistor_t rr) {
    switch (rr) {
        case RR_100:
            HAL_GPIO_WritePin(R0_BUS, R0_PIN, 0);
            HAL_GPIO_WritePin(R1_BUS, R1_PIN, 1);
            return 100.0;
        case RR_5K:
            HAL_GPIO_WritePin(R0_BUS, R0_PIN, 1);
            HAL_GPIO_WritePin(R1_BUS, R1_PIN, 0);
            return 5100.0;
        case RR_100K:
            HAL_GPIO_WritePin(R0_BUS, R0_PIN, 0);
            HAL_GPIO_WritePin(R1_BUS, R1_PIN, 0);
            return 100e3;
        default:
            return 0.0;
    }
}

// Switches is the desired gain resistor
// Returns the resistance (in ohms) of the selected gain resistor
float set_gain_resistor(gain_resistor_t gr) {
    switch (gr) {
        case GR_10K:
            HAL_GPIO_WritePin(G0_BUS, G0_PIN, 0);
            return 10e3;
        case GR_100K:
            HAL_GPIO_WritePin(G0_BUS, G0_PIN, 1);
            return 100e3;
        default:
            return 0.0;
    }   
}
