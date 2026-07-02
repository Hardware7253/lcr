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


rectangular_t polar_to_rectangular(const polar_t *polar);
polar_t rectangular_to_polar(const rectangular_t *rectangular);