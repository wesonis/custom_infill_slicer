///|/ PrusaSlicer is released under the terms of the AGPLv3 or higher
///|/
#ifndef slic3r_FillSchwarzP_hpp_
#define slic3r_FillSchwarzP_hpp_

#include "libslic3r/libslic3r.h"
#include "FillTPMSBase.hpp"

namespace Slic3r {

// Schwarz P (Primitive) TPMS infill.
//   F(x,y,z) = cos(x) + cos(y) + cos(z) = 0
//
// At fixed z = z0 this reduces to
//   cos(y) = -cos(x) - cos(z0)
//   y      = +/- arccos(-cos(x) - cos(z0))   (valid only where |...| <= 1)
//
// Unlike Gyroid/SchwarzD, the isoline does NOT cover all x: there are gaps where
// the right-hand side falls outside [-1, 1]. Each valid x-interval produces TWO
// branches (positive + negative arccos). FillSchwarzP cannot reuse the gyroid wave-tiling
// helpers; its slice generator walks x in fine steps, splits at the +/-1 crossings, and
// emits one polyline per (interval, branch, y0-tile) combination.
class FillSchwarzP : public FillTPMSBase
{
public:
    FillSchwarzP() {}
    Fill* clone() const override { return new FillSchwarzP(*this); }

    // Surface Limits/All_surfs_limits_and_VFs.csv records P's level-set range as [-3, 3].
    // Start with 3.0; calibrate against P_lvl_vf_data.csv if precise density matching needed.
    static constexpr double DensityAdjust = 3.0;

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

#endif // slic3r_FillSchwarzP_hpp_
