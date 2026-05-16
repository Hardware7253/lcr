#include "lcr_math.h"
#include <math.h>

#define PI 3.14159265358979323846F

// Impedance angles in radians
// #define REACTIVE_THRESHOLD 0.78F 
#define REACTIVE_THRESHOLD 0.78F 

// Calculate the passive component from a given complex impedance and frequency
passive_component_t calc_passive_component(polar_t *z, float frequency) {
    passive_component_t passive_component = {0};

    if (z->angle < -REACTIVE_THRESHOLD) {
        passive_component.passive_type = CAPACITOR;
    } else if (z-> angle > REACTIVE_THRESHOLD) {
        passive_component.passive_type = INDUCTOR;
    } else {
        passive_component.passive_type = RESISTOR;
    }

    rectangular_t rectangular_z = polar_to_rectangular(z);
    passive_component.series_resistance = rectangular_z.real; 

    switch (passive_component.passive_type) {
        case RESISTOR:
            passive_component.stray_reactance = rectangular_z.imaginary;
            break;

        case INDUCTOR:
            passive_component.reactive_component_val = rectangular_z.imaginary / (2.0F * PI * frequency);
            break;

        case CAPACITOR:
            passive_component.reactive_component_val = -1 / (2.0F * PI * frequency * rectangular_z.imaginary);
            break;
    }

    return passive_component;
}

// Converts a float to having a unit value
// E.g. 5e-8 > 50n
unit_float_t convert_to_unit(float num) {
    unit_float_t result;

    float abs_num = fabsf(num);

    if (abs_num >= 1.0f) {
        result.unit = ' ';
        result.val = num;
    }
    else if (abs_num >= 1e-3f) {
        result.unit = 'm';
        result.val = num * 1e3f;
    }
    else if (abs_num >= 1e-6f) {
        result.unit = 'u';
        result.val = num * 1e6f;
    }
    else if (abs_num >= 1e-9f) {
        result.unit = 'n';
        result.val = num * 1e9f;
    }
    else if (abs_num >= 1e-12f) {
        result.unit = 'p';
        result.val = num * 1e12f;
    }
    else if (abs_num >= 1e-15f) {
        result.unit = 'f';
        result.val = num * 1e15f;
    }
    else {
        result.unit = ' ';
        result.val = num;
    }

    return result;
}