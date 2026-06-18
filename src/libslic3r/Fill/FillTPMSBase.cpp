///|/ Copyright (c) Prusa Research 2018 - 2021
///|/ PrusaSlicer is released under the terms of the AGPLv3 or higher
///|/
#include <cmath>
#include <algorithm>

#include "../ClipperUtils.hpp"
#include "../ShortestPath.hpp"
#include "FillTPMSBase.hpp"
#include "libslic3r/BoundingBox.hpp"
#include "libslic3r/Point.hpp"
#include "libslic3r/Polygon.hpp"
#include "libslic3r/libslic3r.h"

namespace Slic3r {

void FillTPMSBase::_fill_surface_single(
    const FillParams                &params,
    unsigned int                     /* thickness_layers */,
    const std::pair<float, Point>   & /* direction */,
    ExPolygon                        expolygon,
    Polylines                       &polylines_out)
{
    auto infill_angle = float(this->angle + (CorrectionAngle * 2 * M_PI) / 360.);
    if (std::abs(infill_angle) >= EPSILON)
        expolygon.rotate(-infill_angle);

    BoundingBox bb = expolygon.contour.bounding_box();
    double      density_adjusted = std::max(0., params.density * this->density_adjust_factor());
    coord_t     distance_base    = coord_t(scale_(this->spacing) / density_adjusted);
    coord_t     distance_x       = coord_t(distance_base * params.tpms_period_x);
    coord_t     distance_y       = coord_t(distance_base * params.tpms_period_y);

    // align bounding box to a multiple of our (anisotropic) grid module
    bb.merge(align_to_grid(bb.min,
        Point(coord_t(2 * M_PI * distance_x), coord_t(2 * M_PI * distance_y))));

    // width/height in radian-trig units, already accounting for the per-axis multipliers via distance_{x,y}.
    Polylines polylines = this->compute_world_polylines(
        scale_(this->z),
        density_adjusted,
        this->spacing,
        ceil(bb.size()(0) / distance_x) + 1.,
        ceil(bb.size()(1) / distance_y) + 1.,
        params.tpms_period_x,
        params.tpms_period_y,
        params.tpms_period_z);

    for (Polyline &pl : polylines)
        pl.translate(bb.min);

    polylines = intersection_pl(polylines, expolygon);

    if (! polylines.empty()) {
        const double minlength = scale_(0.8 * this->spacing);
        polylines.erase(
            std::remove_if(polylines.begin(), polylines.end(),
                [minlength](const Polyline &pl) { return pl.length() < minlength; }),
            polylines.end());
    }

    if (! polylines.empty()) {
        size_t polylines_out_first_idx = polylines_out.size();
        if (params.dont_connect())
            append(polylines_out, chain_polylines(polylines));
        else
            Fill::connect_infill(std::move(polylines), expolygon, polylines_out, this->spacing, params);

        if (std::abs(infill_angle) >= EPSILON) {
            for (auto it = polylines_out.begin() + polylines_out_first_idx; it != polylines_out.end(); ++it)
                it->rotate(infill_angle);
        }
    }
}

} // namespace Slic3r
