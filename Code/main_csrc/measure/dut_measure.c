#include "dut_measure.h"
#include <string.h>
#include <math.h>

#define SAMPLES_PER_PERIOD 16  /* ADC can't read fast enough going above 16 samples per period at 93.75KHz */
#define OFFSET_90_SAMPLES (SAMPLES_PER_PERIOD / 4) 
#define SAMPLE_PERIODS 10 
#define SAMPLES (SAMPLES_PER_PERIOD * SAMPLE_PERIODS)

#define TEST_GAIN 10.0 

typedef int32_t buf_t; // int32_t is needed to allow multiplication of 12 bit ADC reads and large accumulators
static buf_t test_samples[SAMPLES] = {0}; // Measured before DUT
static buf_t dut_samples[SAMPLES] = {0};  // Measured after DUT

static bool is_sampling = false;

// Updates min and max from sample buffer and updates the accumulator
static void get_acc_min_max(buf_t buf[], uint16_t buf_len, int64_t *accumulator, buf_t *max, buf_t *min) {
    *min = INT32_MAX;
    *max = INT32_MIN;

    for (uint16_t i = 0; i < buf_len; i++) {
        buf_t sample = buf[i];
        *accumulator += sample;

        if (sample > *max) *max = sample;
        if (sample < *min) *min = sample;
    }
}

// Updates mean, min, and max from sample buffer
// Splits the buffer up into bins and averages the max and min from each of this bins
// buf_len must be divisible by bin_size
static void bin_get_mean_min_max(buf_t buf[], uint16_t buf_len, uint16_t bin_size, buf_t *mean, buf_t *max, buf_t *min) {
    int64_t accumulator = 0;
    int64_t max_accumulator = 0;
    int64_t min_accumulator = 0;
    uint16_t no_bins = buf_len / bin_size;

    for (uint16_t offset = 0; offset < buf_len; offset += bin_size){
        buf_t this_max = 0;
        buf_t this_min = 0;
        get_acc_min_max(buf + offset, bin_size, &accumulator, &this_max, &this_min);

        max_accumulator += this_max;
        min_accumulator += this_min;
    }

    *mean = accumulator / buf_len;
    *max = max_accumulator / no_bins;
    *min = min_accumulator / no_bins;
}

// Adds the offset to every sample in the buffer
static void offset_samples(buf_t buf[], uint16_t buf_len, buf_t offset) {
    for (uint16_t i = 0; i < buf_len; i++) {
        buf[i] += offset;
    }
}

// Calculate impedance from test and dut sample buffers
static polar_t calculate_z(float range_resistor) {
    buf_t test_pk_pk, dut_pk_pk;

    // Remove DC component from ADC readings
    {
        buf_t test_mean, test_max, test_min;
        buf_t dut_mean, dut_max, dut_min;
        bin_get_mean_min_max(test_samples, SAMPLES, SAMPLES_PER_PERIOD, &test_mean, &test_max, &test_min);
        bin_get_mean_min_max(dut_samples, SAMPLES, SAMPLES_PER_PERIOD, &dut_mean, &dut_max, &dut_min);
        offset_samples(test_samples, SAMPLES, -test_mean);
        offset_samples(dut_samples, SAMPLES, -dut_mean);

        test_pk_pk = test_max - test_min;
        dut_pk_pk = dut_max - dut_min;
    }

    volatile int64_t in_phase = 0;
    volatile int64_t quadrature = 0;
    for (uint16_t i = 0; i < SAMPLES; i++) {

        // 90 degrees leading index
        uint16_t j = (i + OFFSET_90_SAMPLES) % SAMPLES;

        in_phase += test_samples[i] * dut_samples[i];
        quadrature += test_samples[j] * dut_samples[i];
    }

    in_phase /= SAMPLES;
    quadrature /= SAMPLES;

    return (polar_t) {
        .mag =  ((float)test_pk_pk * (float)range_resistor) / ((float)dut_pk_pk * (float)TEST_GAIN),
        .angle = -atan2f((float)(quadrature), (float)(in_phase))
    };
}

void init_dut_measurement(void) {
    init_sampling();
}

// Starts measuring the DUT
// Nothing will happen if a measurement operation is already underway
void start_dut_measurement(test_frequency_t test_f) {
    if (!is_sampling) {
        start_sampling(test_f, (uint32_t*)test_samples, (uint32_t*)dut_samples, SAMPLES);
        is_sampling = true;
    }
}

// Attempts to update the z parameter based on the result of the last measurement
// Returns true if the z parameter was set
bool get_dut_measurement(polar_t *z, float range_resistor) {
    if (sample_buffers_full()) {
        *z = calculate_z(range_resistor);
        is_sampling = false;
        return true;
    }
    return false;
}