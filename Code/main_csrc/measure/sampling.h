/*
    Sampling module
    Responsible for sampleing test and DUT waveforms to a buffer
*/
#pragma once

// ADC register callbacks need to be enabled
// Peripheral defines in source


#include <stdint.h>
#include <stdbool.h>

#define SAMPLES_PER_PERIOD 16

typedef enum {
    TF_100KHZ = 0,
    TF_10KHZ,
    TF_1KHZ,
    TF_100HZ,
    NO_OF_TEST_FREQUENCIES,
} test_frequency_t;


uint32_t get_test_frequency_hz(test_frequency_t tf);
void init_sampling(void);
void start_sampling(test_frequency_t test_f, uint32_t *test_buf, uint32_t *dut_buf, uint32_t buf_len);
bool sample_buffers_full(void);