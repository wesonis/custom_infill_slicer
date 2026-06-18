///|/ PrusaSlicer is released under the terms of the AGPLv3 or higher
///|/
#ifndef slic3r_FillSchwarzD_hpp_
#define slic3r_FillSchwarzD_hpp_

#include "libslic3r/libslic3r.h"
#include "FillTPMSBase.hpp"

namespace Slic3r {

// Schwarz D (Diamond) TPMS infill.
//   F(x,y,z) = sin(x)sin(y)sin(z) + sin(x)cos(y)cos(z)
//            + cos(x)sin(y)cos(z) + cos(x)cos(y)sin(z) = 0
//
// At fixed z = z0 this reduces to
//   tan(y) = -sin(x + z0) / cos(x - z0)
// (continuous arctan curve with two branches per 2*pi in y; no gaps).
class FillSchwarzD : public FillTPMSBase
{
public:
    FillSchwarzD() {}
    Fill* clone() const override { return new FillSchwarzD(*this); }

    // Schwarz D level-set range is [-1, 1]; gyroid-equivalent calibration suggests ~2.0
    // gets density mapping in the same ballpark. Refine empirically vs.
    // Lattice Volume Fraction/D_lvl_vf_data.csv if needed.
    static constexpr double DensityAdjust = 2.0;

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

#endif // slic3r_FillSchwarzD_hpp_
