#include "dut_measure.h"

#include "blocking_delay.h"
#include "range_resistor.h"

#include <string.h>
#include <math.h>

#define OFFSET_SAMPLES_90_DEG (SAMPLES_PER_PERIOD / 4) 
#define SAMPLE_PERIODS 10 
#define SAMPLES (SAMPLES_PER_PERIOD * SAMPLE_PERIODS)


typedef int32_t buf_t; // int32_t is needed to allow multiplication of 12 bit ADC reads and large accumulators

// Value of R12
static const float V_TEST_AMP_R1 = 1000.0f;

// Number of measurements used to form an average
static const uint8_t NO_OF_TESTS = 5;

// The signal is clipping if it's pk-pk ADC read exceeds this value
static const buf_t CLIP_PK_PK = 3972; // 3.2V

// Pk-pk less than this is considered noise
static const buf_t NOISE_PK_PK = 62; // 50mV

// If the waveforms are above this threshold when selecting range resistors
// the combination will be accepted instantly
static const buf_t GOOD_PK_PK = 248; // 200mV

// How long to wait before doing anything after changing any relay state
static const uint32_t RELAY_WAIT_TIME = 50; // 50 ms

// How long to wait inbetween independent measurements/tests
static const uint32_t MEASUREMENT_WAIT_TIME = 10; // 10 ms


static buf_t test_samples[SAMPLES] = {0}; // Test waveform samples
static buf_t curr_samples[SAMPLES] = {0}; // DUT current waveform samples

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
// Splits the buffer up into bins and averages the max and min from each of these bins
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

// Calculate impedance from test and current sample buffers
// Assumes the sample buffers are usable
static polar_t calculate_z(float range_resistor, float test_gain_resistor) {
    buf_t test_pk_pk, curr_pk_pk;

    // Remove DC component from ADC readings
    {
        buf_t test_mean, test_max, test_min;
        buf_t curr_mean, curr_max, curr_min;
        bin_get_mean_min_max(test_samples, SAMPLES, SAMPLES_PER_PERIOD, &test_mean, &test_max, &test_min);
        bin_get_mean_min_max(curr_samples, SAMPLES, SAMPLES_PER_PERIOD, &curr_mean, &curr_max, &curr_min);
        offset_samples(test_samples, SAMPLES, -test_mean);
        offset_samples(curr_samples, SAMPLES, -curr_mean);

        test_pk_pk = test_max - test_min;
        curr_pk_pk = curr_max - curr_min;
    }

    int64_t in_phase = 0;
    int64_t quadrature = 0;
    for (uint16_t i = 0; i < SAMPLES; i++) {

        // 90 degrees leading index
        uint16_t j = i + OFFSET_SAMPLES_90_DEG;
        if (j >= SAMPLES) {
            j %= SAMPLES;
        }

        in_phase += test_samples[i] * curr_samples[i];
        quadrature += test_samples[j] * curr_samples[i];
    }

    in_phase /= SAMPLES;
    quadrature /= SAMPLES;

    float v_test_amp_gain = test_gain_resistor / V_TEST_AMP_R1;

    return (polar_t) {
        .mag =  ((float)test_pk_pk * (float)range_resistor) / ((float)curr_pk_pk * v_test_amp_gain),
        .angle = -atan2f((float)(quadrature), (float)(in_phase))
    };
}

void init_dut_measurement(void) {
    init_range_resistors();
    init_sampling();
}

// Starts measuring the DUT
// Nothing will happen if a measurement operation is already underway
// This function does not block
void start_dut_measurement(test_frequency_t test_f) {
    if (!is_sampling) {
        start_sampling(test_f, (uint32_t*)test_samples, (uint32_t*)curr_samples, SAMPLES);
        is_sampling = true;
    }
}

// Attempts to update the z parameter based on the result of the last measurement
// Returns true if the z parameter was set
// This function does not block
bool get_dut_measurement(polar_t *z, float range_resistor, float test_gain_resistor) {
    if (sample_buffers_full()) {
        *z = calculate_z(range_resistor, test_gain_resistor);
        is_sampling = false;
        return true;
    }
    return false;
}

// Finds the correct range and gain resistor to use for the selected test frequency
// This function can block for a long time (hundreds of milliseconds but depends on RELAY_WAIT_TIME)
// Returns GOOD_RANGE or BAD_RANGE depending on the quality of the test and current waveforms
// Returns NO_RANGE if no suitable range / gain resistor combination was found
static range_status_t find_range_gain_resistors(test_frequency_t test_f, range_resistor_t *rr, gain_resistor_t *gr) {
    buf_t best_pk_pk = 0;
    range_resistor_t best_rr, best_gr;
    range_status_t range_status = NO_RANGE;

    for (*gr = 0; *gr < NO_GRS; (*gr)++) {
        for (*rr = 0; *rr < NO_RRS; (*rr)++) {
            set_range_resistor(*rr);
            set_gain_resistor(*gr);
            delay_ms(RELAY_WAIT_TIME);

            start_dut_measurement(test_f);
            while (!sample_buffers_full()) {
                (void)0;
            }
            is_sampling = false;

            // Calculate pk-pk
            buf_t test_pk_pk, curr_pk_pk;
            {
                buf_t test_mean, test_max, test_min;
                buf_t curr_mean, curr_max, curr_min;
                bin_get_mean_min_max(test_samples, SAMPLES, SAMPLES_PER_PERIOD, &test_mean, &test_max, &test_min);
                bin_get_mean_min_max(curr_samples, SAMPLES, SAMPLES_PER_PERIOD, &curr_mean, &curr_max, &curr_min);
                test_pk_pk = test_max - test_min;
                curr_pk_pk = curr_max - curr_min;
            }
            buf_t total_pk_pk = test_pk_pk + curr_pk_pk;

            // Skip over this combination if either signal is clipping or lost in noise
            if (
                test_pk_pk > CLIP_PK_PK ||
                curr_pk_pk > CLIP_PK_PK ||
                test_pk_pk < NOISE_PK_PK ||
                curr_pk_pk < NOISE_PK_PK
            ) {
                continue;
            }

            // Return early if we find an acceptable combination
            if (test_pk_pk > GOOD_PK_PK && curr_pk_pk > GOOD_PK_PK) {
                return GOOD_RANGE;
            }

            // Track the least bad combination
            if (total_pk_pk > best_pk_pk) {
                best_pk_pk = total_pk_pk;
                best_rr = *rr;
                best_gr = *gr;
                range_status = BAD_RANGE;
            }
        }
    }

    // Select the best range resistor combination that was found
    if (range_status == NO_RANGE) {
        set_range_resistor(RR_DEFAULT);
        set_gain_resistor(GR_DEFAULT);
    } else {
        set_range_resistor(best_rr);
        set_gain_resistor(best_gr);
    }

    delay_ms(RELAY_WAIT_TIME);
    return range_status;
}

// Automatically selects the correct range resistor and then
// measures the DUT multiple times and averages the complex impedances
// The impedance result is returned by updating *z
range_status_t measure_dut(polar_t *z, test_frequency_t test_f) {

    // Select range and gain resistor
    range_resistor_t rr;
    gain_resistor_t gr;
    range_status_t range_status = find_range_gain_resistors(test_f, &rr, &gr);

    if (range_status == NO_RANGE) {
        return range_status;
    }

    // Have to set the range resistors again just to get their values
    float rr_val = set_range_resistor(rr);
    float gr_val = set_gain_resistor(gr);
    delay_ms(RELAY_WAIT_TIME);

    z->angle = 0;
    z->mag = 0;
    polar_t z_result;

    // Perform tests
    for (uint8_t i = 0; i < NO_OF_TESTS; i++) {
       start_dut_measurement(test_f);

       // Block until the measurement is complete
        while(!get_dut_measurement(&z_result, rr_val, gr_val)) {
            (void)0;
        }

        z->mag += z_result.mag;
        z->angle += z_result.angle;
        delay_ms(MEASUREMENT_WAIT_TIME);
    }

    // Reset range resistors to default state (coil unpowered)
    set_range_resistor(RR_DEFAULT);
    set_gain_resistor(GR_DEFAULT);

    // Average
    z->mag /= NO_OF_TESTS;
    z->angle /= NO_OF_TESTS;

    return range_status;
}