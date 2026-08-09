#include "dut_measure.h"

#include "blocking_delay.h"
#include "range_resistor.h"

#include <string.h>
#include <math.h>

#define OFFSET_SAMPLES_90_DEG (SAMPLES_PER_PERIOD / 4) 
#define SAMPLE_PERIODS 20 
#define SAMPLES (SAMPLES_PER_PERIOD * SAMPLE_PERIODS)


typedef int32_t buf_t; // int32_t is needed to allow multiplication of 12 bit ADC reads and large accumulators

// Value of R12
// v_test inverting amplifier input resistor value
static const float V_TEST_AMP_R1 = 1000.0f; // Ω

// Number of measurements used to form an average
static const uint8_t NO_OF_TESTS = 5;

// The signal is clipping if it's pk-pk ADC read exceeds this value
static const buf_t CLIP_PK_PK = 4066; // 3.276V

// If the waveforms are above this threshold when selecting range resistors
// the combination will be accepted instantly
static const buf_t GOOD_PK_PK = 500; // 403mV

// How long to wait before doing anything after changing any relay state
static const uint32_t RELAY_WAIT = 50; // ms

// How long to wait inbetween independent measurements/tests
static const uint32_t MEASUREMENT_WAIT = 50; // ms

// How long to wait after starting the DDS to start sampling
static const uint32_t DDS_WAIT = 50; // ms

// Expected number of zero crossings in the sample buffers + margin for noise
static const uint16_t EXPECTED_CROSSINGS = (2 * SAMPLE_PERIODS) + (SAMPLE_PERIODS / 4);

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

// Returns the number of times the samples in the buffer cross the cross_point
static uint16_t get_crossings(buf_t cross_point, buf_t buf[], uint16_t buf_len) {
    uint16_t crossings = 0;
    if (buf_len < 2) {
        return crossings;
    }

    buf_t last_val = buf[0];
    for (uint16_t i = 1; i < buf_len; i++) {
        buf_t this_val = buf[i];

        if (
            (last_val < cross_point && this_val >= cross_point) ||
            (last_val > cross_point && this_val <= cross_point)
        ) {
            crossings++;
        }
        last_val = this_val;
    }
    return crossings;
}

// Finds the correct range and gain resistor to use for the selected test frequency
// This function can block for a long time (hundreds of milliseconds but depends on RELAY_WAIT_TIME)
// Returns GOOD_RANGE or BAD_RANGE depending on the quality of the test and current waveforms
// Returns NO_RANGE if no suitable range / gain resistor combination was found
static range_status_t find_range_gain_resistors(test_frequency_t test_f, range_resistor_t *rr, gain_resistor_t *gr) {
    buf_t best_pk_pk = 0;
    range_resistor_t best_rr, best_gr;
    range_status_t range_status = NO_RANGE;
    start_dds(test_f);
    delay_ms(DDS_WAIT);

    for (*gr = 0; *gr < NO_GRS; (*gr)++) {
        for (*rr = 0; *rr < NO_RRS; (*rr)++) {
            set_range_resistor(*rr);
            set_gain_resistor(*gr);
            delay_ms(RELAY_WAIT);

            start_dut_measurement(test_f);
            while (!sample_buffers_full()) {
                (void)0;
            }
            is_sampling = false;

            // Calculate pk-pk
            buf_t test_pk_pk, test_mean;
            buf_t curr_pk_pk, curr_mean;
            {
                buf_t test_max, test_min;
                buf_t curr_max, curr_min;
                bin_get_mean_min_max(test_samples, SAMPLES, SAMPLES_PER_PERIOD, &test_mean, &test_max, &test_min);
                bin_get_mean_min_max(curr_samples, SAMPLES, SAMPLES_PER_PERIOD, &curr_mean, &curr_max, &curr_min);
                test_pk_pk = test_max - test_min;
                curr_pk_pk = curr_max - curr_min;
            }
            buf_t total_pk_pk = test_pk_pk + curr_pk_pk;

            // This combination is bad if there are too many zero crossings (signal is noisy)
            // Combination is also bad if the signals are clipping
            if (
                get_crossings(test_mean, test_samples, SAMPLES) > EXPECTED_CROSSINGS ||
                get_crossings(curr_mean, curr_samples, SAMPLES) > EXPECTED_CROSSINGS ||
                test_pk_pk > CLIP_PK_PK ||
                curr_pk_pk > CLIP_PK_PK
            ) {
                continue;
            }

            // Return early if we find an acceptable combination
            if (test_pk_pk > GOOD_PK_PK && curr_pk_pk > GOOD_PK_PK) {
                stop_dds();
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
    *rr = best_rr;
    *gr = best_gr;

    stop_dds();
    delay_ms(RELAY_WAIT);
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
    delay_ms(RELAY_WAIT);

    z->angle = 0;
    z->mag = 0;
    polar_t z_result;

    start_dds(test_f);
    delay_ms(DDS_WAIT);

    // Perform tests
    for (uint8_t i = 0; i < NO_OF_TESTS; i++) {
       start_dut_measurement(test_f);

       // Block until the measurement is complete
        while(!get_dut_measurement(&z_result, rr_val, gr_val)) {
            (void)0;
        }

        z->mag += z_result.mag;
        z->angle += z_result.angle;
        delay_ms(MEASUREMENT_WAIT);
    }

    stop_dds();

    // Reset range resistors to default state (coil unpowered)
    set_range_resistor(RR_DEFAULT);
    set_gain_resistor(GR_DEFAULT);

    // Average
    z->mag /= NO_OF_TESTS;
    z->angle /= NO_OF_TESTS;

    return range_status;
}