#include "complex.h"
#include <math.h>

rectangular_t polar_to_rectangular(polar_t *polar) {
    rectangular_t rectangular;
    rectangular.imaginary = polar->mag * sinf(polar->angle);
    rectangular.real = polar->mag * cosf(polar->angle);
    return rectangular;
}

polar_t rectangular_to_polar(rectangular_t *rectangular) {
    polar_t polar;
    polar.mag = sqrtf(rectangular->imaginary * rectangular->imaginary + rectangular->real * rectangular->real);
    polar.angle = atan2f(rectangular->imaginary, rectangular->real);
    return polar;
}