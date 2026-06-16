/*
    Sampling module
    Responsible for sampleing test and DUT waveforms to a buffer
*/
#pragma once

// ADC register callbacks need to be enabled
// Peripheral defines in source


#include <stdint.h>
#include <stdbool.h>

#define SAMPLES_PER_PERIOD 16  /* ADC can't read fast enough going above 16 samples per period at 93.75KHz */

typedef enum {
    TF_100KHZ, // Higher than 94 MHz isn't possible without using ADC_SAMPLETIME_3CYCLES
    TF_10KHZ,
    TF_1KHZ,
    TF_100HZ,
} test_frequency_t;

void init_sampling(void);
void start_sampling(test_frequency_t test_f, uint32_t *test_buf, uint32_t *dut_buf, uint32_t buf_len);
bool sample_buffers_full(void);