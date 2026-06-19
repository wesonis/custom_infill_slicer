///|/ Copyright (c) Prusa Research 2018 - 2021
///|/ PrusaSlicer is released under the terms of the AGPLv3 or higher
///|/
#ifndef slic3r_FillTPMSBase_hpp_
#define slic3r_FillTPMSBase_hpp_

#include <cmath>
#include <algorithm>
#include <utility>
#include <vector>

#include "libslic3r/libslic3r.h"
#include "FillBase.hpp"
#include "libslic3r/ExPolygon.hpp"
#include "libslic3r/Point.hpp"
#include "libslic3r/Polyline.hpp"

namespace Slic3r {

// Shared scaffolding for analytic 2D-slice TPMS infills (Gyroid, Schwarz P, Schwarz D, ...).
// Each subclass provides:
//   - density_adjust_factor() : the empirical density-to-spacing constant for its family.
//   - compute_world_polylines(): the per-family analytic slice math, returning polylines in
//                                scaled world coords relative to the bb origin.
//
// The base handles rotation, anisotropic bounding-box alignment, intersection_pl, the small-
// segment filter, connect_infill, and the final unrotation step. Two templated helpers
// (make_wave + make_one_period) factor out the wave-tiling code shared by single-valued-y(x)
// families (Gyroid, Schwarz D).
class FillTPMSBase : public Fill
{
public:
    // Correction applied to the user-set infill angle to maximize printing speed at default.
    static constexpr float CorrectionAngle = -45.f;
    // Pattern upper resolution tolerance (mm^-2).
    static constexpr double PatternTolerance = 0.2;

    // Most TPMS patterns hang in air per layer; suppress bridge flow handling.
    bool use_bridge_flow() const override { return false; }
    bool is_self_crossing()      override { return false; }

protected:
    void _fill_surface_single(
        const FillParams                &params,
        unsigned int                     thickness_layers,
        const std::pair<float, Point>   &direction,
        ExPolygon                        expolygon,
        Polylines                       &polylines_out) override;

    virtual double density_adjust_factor() const = 0;

    // Returns polylines in scaled world coords relative to the bb origin (no translation yet;
    // the base translates by bb.min, intersects with the expolygon, runs connect_infill, and
    // rotates back into the unrotated frame).
    //   gridZ            : scale_(this->z), scaled world Z of the current layer.
    //   density_adjusted : params.density * this->density_adjust_factor(), clamped >= 0.
    //   line_spacing     : this->spacing (unscaled mm).
    //   width, height    : bb extents in radian-trig units (= bb.size()/distance_{x,y}).
    //   mx, my, mz       : per-axis period multipliers (default 1.0).
    virtual Polylines compute_world_polylines(
        double gridZ,
        double density_adjusted,
        double line_spacing,
        double width,
        double height,
        double mx, double my, double mz) const = 0;

    // ---- Shared helpers for single-valued y(x) TPMS families (Gyroid, Schwarz D) ----
    // IsolineFn signature: double f(double x, double z_sin, double z_cos, bool vertical, bool flip).

    template<typename IsolineFn>
    static inline Polyline make_wave(
        IsolineFn               f,
        const std::vector<Vec2d> &one_period,
        double width, double height, double offset, double scaleFactor,
        double z_cos, double z_sin, bool vertical, bool flip,
        double mx, double my)
    {
        std::vector<Vec2d> points = one_period;
        double period = points.back()(0);
        if (width != period) {
            points.reserve(one_period.size() * size_t(floor(width / period)));
            points.pop_back();
            size_t n = points.size();
            do {
                points.emplace_back(points[points.size() - n].x() + period,
                                    points[points.size() - n].y());
            } while (points.back()(0) < width - EPSILON);
            points.emplace_back(Vec2d(width, f(width, z_sin, z_cos, vertical, flip)));
        }

        Polyline polyline;
        polyline.points.reserve(points.size());
        for (auto &point : points) {
            point(1) += offset;
            point(1) = std::clamp(double(point.y()), 0., height);
            if (vertical)
                std::swap(point(0), point(1));
            // Preserve the Eigen `point * scaleFactor` form so defaults (mx=my=1) keep
            // identical FP path; apply per-axis multipliers afterwards in place.
            Vec2d scaled = point * scaleFactor;
            scaled.x() *= mx;
            scaled.y() *= my;
            polyline.points.emplace_back(scaled.cast<coord_t>());
        }
        return polyline;
    }

    template<typename IsolineFn>
    static inline std::vector<Vec2d> make_one_period(
        IsolineFn f,
        double width, double scaleFactor, double z_cos, double z_sin,
        bool vertical, bool flip, double tolerance)
    {
        std::vector<Vec2d> points;
        double dx = M_PI_2;
        double limit = std::min(2 * M_PI, width);
        points.reserve(coord_t(ceil(limit / tolerance / 3)));

        for (double x = 0.; x < limit - EPSILON; x += dx)
            points.emplace_back(Vec2d(x, f(x, z_sin, z_cos, vertical, flip)));
        points.emplace_back(Vec2d(limit, f(limit, z_sin, z_cos, vertical, flip)));

        for (;;) {
            size_t size = points.size();
            for (unsigned int i = 1; i < size; ++i) {
                auto &lp = points[i - 1];
                auto &rp = points[i];
                double x = lp(0) + (rp(0) - lp(0)) / 2;
                double y = f(x, z_sin, z_cos, vertical, flip);
                Vec2d ip = {x, y};
                if (std::abs(cross2(Vec2d(ip - lp), Vec2d(ip - rp))) > sqr(tolerance))
                    points.emplace_back(std::move(ip));
            }
            if (size == points.size())
                break;
            std::sort(points.begin(), points.end(),
                [](const Vec2d &lhs, const Vec2d &rhs) { return lhs(0) < rhs(0); });
        }
        return points;
    }
};

} // namespace Slic3r

#endif // slic3r_FillTPMSBase_hpp_
