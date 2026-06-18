///|/ Copyright (c) Prusa Research 2018 - 2020 Vojtěch Bubník @bubnikv, Lukáš Matěna @lukasmatena
///|/ Copyright (c) SuperSlicer 2018 Remi Durand @supermerill
///|/
///|/ PrusaSlicer is released under the terms of the AGPLv3 or higher
///|/
#ifndef slic3r_FillGyroid_hpp_
#define slic3r_FillGyroid_hpp_

#include "libslic3r/libslic3r.h"
#include "FillTPMSBase.hpp"

namespace Slic3r {

class FillGyroid : public FillTPMSBase
{
public:
    FillGyroid() {}
    Fill* clone() const override { return new FillGyroid(*this); }

    // Density adjustment to map fill_density % to an equivalent solid-volume fraction
    // for the gyroid level-set. Tuned empirically.
    static constexpr double DensityAdjust = 2.44;

protected:
    double density_adjust_factor() const override { return DensityAdjust; }

    Polylines compute_world_polylines(
        double gridZ,
        double density_adjusted,
        double line_spacing,
        double width,
        double height,
        double mx, double my, double mz) const override;
};

} // namespace Slic3r

#endif // slic3r_FillGyroid_hpp_
