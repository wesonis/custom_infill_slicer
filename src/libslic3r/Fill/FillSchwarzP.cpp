///|/ PrusaSlicer is released under the terms of the AGPLv3 or higher
///|/
#include <cmath>
#include <algorithm>
#include <vector>

#include "FillSchwarzP.hpp"
#include "libslic3r/Point.hpp"
#include "libslic3r/Polyline.hpp"
#include "libslic3r/libslic3r.h"

namespace Slic3r {

namespace {
inline Point to_world(double x_rad, double y_rad, double scaleFactor, double mx, double my)
{
    Vec2d scaled(x_rad, y_rad);
    scaled *= scaleFactor;
    scaled.x() *= mx;
    scaled.y() *= my;
    return scaled.cast<coord_t>();
}
} // anon

Polylines FillSchwarzP::compute_world_polylines(
    double gridZ, double density_adjusted, double line_spacing,
    double width, double height, double mx, double my, double mz) const
{
    const double scaleFactor = scale_(line_spacing) / density_adjusted;

    // Radian-space chord tolerance; mirrors the gyroid/SchwarzD path.
    const double tolerance = std::min(line_spacing / 2, FillTPMSBase::PatternTolerance)
                             / unscale<double>(scaleFactor);

    const double z0    = gridZ / (scaleFactor * mz);
    const double cos_z = cos(z0);

    // Sweep step in radian-trig units. Floor at 0.01 rad to keep polyline counts bounded
    // for extreme tolerance settings.
    const double dx = std::max(0.01, tolerance);

    Polylines result;

    // For each y0 tile. Schwarz P repeats with period 2*pi in y.
    // Start one tile before 0 so the bb's lower edge is covered after intersection_pl.
    for (double y0 = -2 * M_PI; y0 < height + EPSILON; y0 += 2 * M_PI) {

        // Per y0-tile, scan x and emit one upper + one lower polyline per valid interval.
        std::vector<Vec2d> buf_upper, buf_lower;

        auto flush = [&] {
            if (buf_upper.size() < 2)
                { buf_upper.clear(); buf_lower.clear(); return; }
            Polyline up, lo;
            up.points.reserve(buf_upper.size());
            lo.points.reserve(buf_lower.size());
            for (const auto &p : buf_upper)
                up.points.emplace_back(to_world(p.x(), p.y(), scaleFactor, mx, my));
            for (const auto &p : buf_lower)
                lo.points.emplace_back(to_world(p.x(), p.y(), scaleFactor, mx, my));
            result.emplace_back(std::move(up));
            result.emplace_back(std::move(lo));
            buf_upper.clear();
            buf_lower.clear();
        };

        double prev_x = 0.;
        double prev_g = -cos(0.) - cos_z;
        bool   prev_valid = (prev_g >= -1.0 && prev_g <= 1.0);
        if (prev_valid) {
            const double y = acos(std::clamp(prev_g, -1.0, 1.0));
            buf_upper.emplace_back(prev_x,  y + y0);
            buf_lower.emplace_back(prev_x, -y + y0 + 2 * M_PI);
        }

        for (double x = dx; x < width + EPSILON; x += dx) {
            const double g     = -cos(x) - cos_z;
            const bool   valid = (g >= -1.0 && g <= 1.0);

            if (valid && prev_valid) {
                const double y = acos(std::clamp(g, -1.0, 1.0));
                buf_upper.emplace_back(x,  y + y0);
                buf_lower.emplace_back(x, -y + y0 + 2 * M_PI);
            } else if (valid && !prev_valid) {
                // Just entered a valid interval. Linearly interpolate the crossing where g hits +/-1.
                const double target = (prev_g < -1.0) ? -1.0 : 1.0;
                const double t      = (target - prev_g) / (g - prev_g);
                const double xc     = prev_x + t * (x - prev_x);
                buf_upper.emplace_back(xc, 0.0 + y0 + (target ==  1.0 ? 0.0 : M_PI));
                buf_lower.emplace_back(xc, 0.0 + y0 + 2 * M_PI - (target == 1.0 ? 0.0 : M_PI));
                const double y = acos(std::clamp(g, -1.0, 1.0));
                buf_upper.emplace_back(x,  y + y0);
                buf_lower.emplace_back(x, -y + y0 + 2 * M_PI);
            } else if (!valid && prev_valid) {
                // Just left a valid interval. Linearly interpolate the crossing.
                const double target = (g < -1.0) ? -1.0 : 1.0;
                const double t      = (target - prev_g) / (g - prev_g);
                const double xc     = prev_x + t * (x - prev_x);
                buf_upper.emplace_back(xc, 0.0 + y0 + (target ==  1.0 ? 0.0 : M_PI));
                buf_lower.emplace_back(xc, 0.0 + y0 + 2 * M_PI - (target == 1.0 ? 0.0 : M_PI));
                flush();
            }
            // both invalid: nothing to do.

            prev_x     = x;
            prev_g     = g;
            prev_valid = valid;
        }
        flush();
    }

    return result;
}

} // namespace Slic3r
