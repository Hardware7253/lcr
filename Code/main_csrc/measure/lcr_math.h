#pragma once

#include "complex.h"

typedef enum {
    RESISTOR,
    INDUCTOR,
    CAPACITOR
} passive_type_t;

typedef struct {
    float series_resistance;
    float stray_reactance;
    float reactive_component_val; // Either inductance or capacitance
    passive_type_t passive_type;
} passive_component_t;

typedef struct {
    char unit;
    float val;
} unit_float_t;

passive_component_t calc_passive_component(polar_t *z, float frequency);
unit_float_t convert_to_unit(float num);