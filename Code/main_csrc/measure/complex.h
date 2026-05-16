#pragma once

#include <stdint.h>

typedef struct {
    float mag;
    float angle;
} polar_t;

typedef struct {
    float real;
    float imaginary;
} rectangular_t;


float rad_to_degrees(float radians);
rectangular_t polar_to_rectangular(polar_t *polar);
polar_t rectangular_to_polar(rectangular_t *rectangular);





