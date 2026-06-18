///|/ PrusaSlicer is released under the terms of the AGPLv3 or higher
///|/
#include <cmath>
#include <algorithm>
#include <vector>

#include "FillSchwarzD.hpp"
#include "libslic3r/Point.hpp"
#include "libslic3r/Polyline.hpp"
#include "libslic3r/libslic3r.h"

namespace Slic3r {

// Analytic 2D-slice for Schwarz D at fixed z = z0.
//
// Starting from
//   F = sin(x) sin(y) sin(z0) + sin(x) cos(y) cos(z0)
//     + cos(x) sin(y) cos(z0) + cos(x) cos(y) sin(z0) = 0,
// grouping the y-dependent terms gives
//   sin(y) * cos(x - z0) + cos(y) * sin(x + z0) = 0
//     => tan(y) = -sin(x + z0) / cos(x - z0)
//     => y     = atan2(-sin(x + z0), cos(x - z0))                 (principal branch)
//
// `vertical` and `flip` mirror the gyroid's convention to cover the four sectors of the
// unit cell smoothly. The flip toggle shifts by +pi to pick the second branch.
static inline double schwarzd_f(double x, double z_sin, double z_cos, bool vertical, bool flip)
{
    // sin(x + z0) = sin(x)*cos(z0) + cos(x)*sin(z0)
    // cos(x - z0) = cos(x)*cos(z0) + sin(x)*sin(z0)
    const double sx = sin(x);
    const double cx = cos(x);
    double num = -(sx * z_cos + cx * z_sin);
    double den =   cx * z_cos + sx * z_sin;
    if (vertical) {
        // In vertical orientation the roles of (sin,cos) of x swap; we re-express by phase shift
        // and let the base class handle the world-frame swap.
        std::swap(num, den);
        num = -num;
    }
    double y = std::atan2(num, den);
    if (flip)
        y += M_PI;
    // Shift into [0, 2*pi) to match the gyroid's convention so the make_wave clamp to [0, height]
    // gives well-behaved polylines.
    if (y < 0.)
        y += 2 * M_PI;
    return y;
}

Polylines FillSchwarzD::compute_world_polylines(
    double gridZ, double density_adjusted, double line_spacing,
    double width, double height, double mx, double my, double mz) const
{
    const double scaleFactor = scale_(line_spacing) / density_adjusted;
    const double tolerance = std::min(line_spacing / 2, FillTPMSBase::PatternTolerance)
                             / unscale<double>(scaleFactor);

    const double z     = gridZ / (scaleFactor * mz);
    const double z_sin = sin(z);
    const double z_cos = cos(z);

    bool   vertical    = (std::abs(z_sin) <= std::abs(z_cos));
    double lower_bound = 0.;
    double upper_bound = height;
    bool   flip        = false;
    if (vertical) {
        lower_bound = -M_PI;
        upper_bound = width - M_PI_2;
        std::swap(width, height);
    }

    auto one_period_odd  = make_one_period(&schwarzd_f, width, scaleFactor, z_cos, z_sin, vertical, flip, tolerance);
    flip = !flip;
    auto one_period_even = make_one_period(&schwarzd_f, width, scaleFactor, z_cos, z_sin, vertical, flip, tolerance);

    Polylines result;
    for (double y0 = lower_bound; y0 < upper_bound + EPSILON; y0 += M_PI) {
        result.emplace_back(make_wave(&schwarzd_f, one_period_odd, width, height, y0, scaleFactor,
                                      z_cos, z_sin, vertical, flip, mx, my));
        y0 += M_PI;
        if (y0 < upper_bound + EPSILON)
            result.emplace_back(make_wave(&schwarzd_f, one_period_even, width, height, y0, scaleFactor,
                                          z_cos, z_sin, vertical, flip, mx, my));
    }
    return result;
}

} // namespace Slic3r
