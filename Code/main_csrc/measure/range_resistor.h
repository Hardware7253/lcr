#pragma once

// Defaults are the range/gain resistors that are selected without
// applying any voltage to the selector relay coils
#define RR_DEFAULT RR_100K
#define GR_DEFAULT GR_10K

// Values the main range resistors can take
// Ordered from most to least likely to find the correct range (most likely = 5K)
typedef enum {
    RR_5K = 0,
    RR_100,
    RR_100K,
    NO_RRS,
} range_resistor_t;

// Values the gain resistor can take for the test voltage amplifier opamp
// Ordered from most to least likely to find the correct range (most likely = 10K)
typedef enum {
    GR_10K = 0,
    GR_100K,
    NO_GRS,
} gain_resistor_t;

void init_range_resistors(void);
float set_range_resistor(range_resistor_t rr);
float set_gain_resistor(gain_resistor_t gr);