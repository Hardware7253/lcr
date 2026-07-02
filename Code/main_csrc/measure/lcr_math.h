#pragma once

#include "complex.h"

typedef enum {
    RESISTOR,
    INDUCTOR,
    CAPACITOR
} passive_type_t;

typedef struct {
    rectangular_t impedance;
    float reactive_component_val; // Either inductance or capacitance
    passive_type_t passive_type;
} passive_component_t;

passive_component_t calc_passive_component(const polar_t *z, float frequency);
float rad_to_degrees(float radians);