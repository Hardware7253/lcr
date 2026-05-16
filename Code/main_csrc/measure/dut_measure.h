#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "sampling.h"
#include "complex.h"


void init_dut_measurement(void);
void start_dut_measurement(test_frequency_t test_f);
bool get_dut_measurement(polar_t *z, float range_resistor);