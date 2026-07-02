#include "lcr_math.h"
#include <math.h>

#define PI 3.14159265358979323846F

// Complex impedance angle in radians before a component is considered reactive
#define REACTIVE_THRESHOLD 0.78F

// Calculate the passive component from a given complex impedance and frequency
passive_component_t calc_passive_component(const polar_t *z, float frequency) {
    passive_component_t passive_component = {0};

    if (z->angle < -REACTIVE_THRESHOLD) {
        passive_component.passive_type = CAPACITOR;
    } else if (z-> angle > REACTIVE_THRESHOLD) {
        passive_component.passive_type = INDUCTOR;
    } else {
        passive_component.passive_type = RESISTOR;
    }

    passive_component.impedance = polar_to_rectangular(z);

    switch (passive_component.passive_type) {
        case INDUCTOR:
            passive_component.reactive_component_val = passive_component.impedance.imaginary / (2.0F * PI * frequency);
            break;

        case CAPACITOR:
            passive_component.reactive_component_val = -1 / (2.0F * PI * frequency * passive_component.impedance.imaginary);
            break;
        
        default:
            break;
    }

    return passive_component;
}

float rad_to_degrees(float radians) {
    return radians * (180.0 / PI);
}