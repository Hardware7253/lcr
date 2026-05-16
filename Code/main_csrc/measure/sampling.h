/*
    Sampling module
    Responsible for sampleing test and DUT waveforms to a buffer
*/
#pragma once

// ADC register callbacks need to be enabled
// Peripheral defines in source


#include <stdint.h>
#include <stdbool.h>

#define DEBUG_SAMPLING 0

typedef enum {
    TF_94KHZ = 0, // Higher than 94 MHz isn't possible without using ADC_SAMPLETIME_3CYCLES
    TF_10KHZ = 1,
    TF_1KHZ = 2,
    #if (DEBUG_SAMPLING == 1)
    TF_DEBUG = 4, // 10 Hz
    #endif
} test_frequency_t;


void init_sampling(void);
void start_sampling(test_frequency_t test_f, uint32_t *test_buf, uint32_t *dut_buf, uint32_t buf_len);
bool sample_buffers_full(void);