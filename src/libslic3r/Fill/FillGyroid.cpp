///|/ Copyright (c) Prusa Research 2018 - 2021 Vojtěch Bubník @bubnikv, Lukáš Matěna @lukasmatena, Enrico Turri @enricoturri1966
///|/ Copyright (c) SuperSlicer 2018 - 2019 Remi Durand @supermerill
///|/
///|/ PrusaSlicer is released under the terms of the AGPLv3 or higher
///|/
#include <cmath>
#include <algorithm>
#include <vector>

#include "FillGyroid.hpp"
#include "libslic3r/Point.hpp"
#include "libslic3r/Polyline.hpp"
#include "libslic3r/libslic3r.h"

namespace Slic3r {

// Analytic 2D-slice inversion of the gyroid surface
//   sin(x) cos(y) + sin(y) cos(z) + sin(z) cos(x) = 0
// At fixed z = z0: with A = sin(x), B = z_cos, C = -z_sin*cos(x), we solve A cos(y) + B sin(y) = C
// via the Weierstrass-style asin chain. Two orientations (`vertical`) and two phase flips
// (`flip`) cover the four sectors of the unit cell.
static inline double gyroid_f(double x, double z_sin, double z_cos, bool vertical, bool flip)
{
    if (vertical) {
        double phase_offset = (z_cos < 0 ? M_PI : 0) + M_PI;
        double a   = sin(x + phase_offset);
        double b   = -z_cos;
        double res = z_sin * cos(x + phase_offset + (flip ? M_PI : 0.));
        double r   = sqrt(sqr(a) + sqr(b));
        return asin(a / r) + asin(res / r) + M_PI;
    } else {
        double phase_offset = z_sin < 0 ? M_PI : 0.;
        double a   = cos(x + phase_offset);
        double b   = -z_sin;
        double res = z_cos * sin(x + phase_offset + (flip ? 0 : M_PI));
        double r   = sqrt(sqr(a) + sqr(b));
        return (asin(a / r) + asin(res / r) + 0.5 * M_PI);
    }
}

Polylines FillGyroid::compute_world_polylines(
    double gridZ, double density_adjusted, double line_spacing,
    double width, double height, double mx, double my, double mz) const
{
    const double scaleFactor = scale_(line_spacing) / density_adjusted;

    // tolerance in radian units; clamp the max as there's no benefit beyond a point.
    const double tolerance = std::min(line_spacing / 2, FillTPMSBase::PatternTolerance)
                             / unscale<double>(scaleFactor);

    // Z multiplier stretches the gyroid's Z period: larger mz -> slower trig-z change with world-z.
    const double z     = gridZ / (scaleFactor * mz);
    const double z_sin = sin(z);
    const double z_cos = cos(z);

    bool   vertical    = (std::abs(z_sin) <= std::abs(z_cos));
    double lower_bound = 0.;
    double upper_bound = height;
    bool   flip        = true;
    if (vertical) {
        flip        = false;
        lower_bound = -M_PI;
        upper_bound = width - M_PI_2;
        std::swap(width, height);
    }

    auto one_period_odd  = make_one_period(&gyroid_f, width, scaleFactor, z_cos, z_sin, vertical, flip, tolerance);
    flip = !flip;
    auto one_period_even = make_one_period(&gyroid_f, width, scaleFactor, z_cos, z_sin, vertical, flip, tolerance);

    Polylines result;
    for (double y0 = lower_bound; y0 < upper_bound + EPSILON; y0 += M_PI) {
        result.emplace_back(make_wave(&gyroid_f, one_period_odd, width, height, y0, scaleFactor,
                                      z_cos, z_sin, vertical, flip, mx, my));
        y0 += M_PI;
        if (y0 < upper_bound + EPSILON)
            result.emplace_back(make_wave(&gyroid_f, one_period_even, width, height, y0, scaleFactor,
                                          z_cos, z_sin, vertical, flip, mx, my));
    }
    return result;
}

} // namespace Slic3r
