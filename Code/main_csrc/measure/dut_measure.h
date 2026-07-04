#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "sampling.h"
#include "complex.h"

typedef enum {
    NO_RANGE = 0,
    BAD_RANGE,
    GOOD_RANGE,
} range_status_t;


void init_dut_measurement(void);
void start_dut_measurement(test_frequency_t test_f);
bool get_dut_measurement(polar_t *z, float range_resistor, float test_gain_resistor);
range_status_t measure_dut(polar_t *z, test_frequency_t test_f);